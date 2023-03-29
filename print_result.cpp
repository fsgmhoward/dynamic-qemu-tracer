// Example: 
#define _DEBUG_

#include <sys/stat.h>
#include <sys/mman.h>
#include "schema_io.hpp"

extern "C" {
    #include <Zydis/Zydis.h>
}

using namespace std;

// Ref: https://dev.to/namantam1/ways-to-get-the-file-size-in-c-2mag
int64_t get_file_size(char *filename) {
    struct stat file_status;
    if (stat(filename, &file_status) < 0) {
        return -1;
    }
    return file_status.st_size;
}

int main(int argc, char ** argv) {
    if (argc < 4) {
        cout << "Usage: ./print <executable> <dynamic.bin> <output.txt>" << endl;
        exit(-1);
    }
    
    // Open output file
    ofstream of(argv[3], ofstream::trunc);
    
    // Open executable binary
    int exe_fd = open(argv[1], O_RDONLY);
    if (exe_fd == -1) {
        perror("Error opening executable file");
        return -1;
    }
    int64_t exe_size = get_file_size(argv[1]);
    
    uint8_t * exe = (uint8_t *) mmap(nullptr, exe_size, PROT_READ, MAP_SHARED, exe_fd, 0);
    if (exe == MAP_FAILED) {
        perror("Error mapping executable file");
        return -1;
    }
    of << "# Successfully opened executable file" << endl;
    of << "# Input file size = " << exe_size << " bytes" << endl;
    
    // Open capnproto capture file
    map<int64_t, int8_t> dynamic_offsets;
    int64_t base_address;
    string digest;
    read_version_1(argv[2], dynamic_offsets, base_address, digest);
    of << "# " << endl;
    of << "# Original binary digest = " << digest << endl;
    of << "# Base address = 0x" << hex << base_address << dec << endl;
    of << "# Finished reading. Total #records = " << dynamic_offsets.size() << endl;
    
    int64_t max_address = dynamic_offsets.rbegin()->first + base_address;
    int8_t num_digits = 0;
    while (max_address != 0) {
        max_address /= 16;
        ++num_digits;
    }
    
    int8_t max_length = 0;
    for (auto [offset, length] : dynamic_offsets) {
        max_length = max(max_length, length);
    }
    
    // Disassemble
    for (auto [offset, length] : dynamic_offsets) {
        ZydisDisassembledInstruction instruction;
        int64_t runtime_addr = base_address + offset;
        
        if (ZYAN_SUCCESS(ZydisDisassembleIntel( 
           /* machine_mode:    */ ZYDIS_MACHINE_MODE_LONG_64, 
           /* runtime_address: */ runtime_addr, 
           /* buffer:          */ exe + offset, 
           /* length:          */ length, 
           /* instruction:     */ &instruction 
        ))) {
            // Print runtime address
            of << setw(num_digits) << hex << runtime_addr << ": ";
            // Print raw bytes, still in hex mode
            of << setfill('0');
            for (int8_t i = 0; i < length; ++i) {
                of << setw(2) << (int) *(exe + offset + i) << ' ';
            }
            // Print remaining spaces and reset mode
            of << setfill(' ') << setw((max_length - length) * 3) << ' ' << dec;
            // Print instruction
            of << instruction.text << endl;
        } else {
            of << "# Unknown error in disassembly at 0x" << hex << runtime_addr << dec << endl;
        }
    }
    
    // Free resources
    if (munmap(exe, exe_size) == -1) {
        perror("Error unmapping executable file");
        return -1;
    }
}
