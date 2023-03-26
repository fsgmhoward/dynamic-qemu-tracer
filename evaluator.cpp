/**
 * This program evaluates the static disassembly result against dynamic disassembly (traced) result
 * By default, static disassembly uses version 0 output and dynamic one uses version 1 output
 */
 
#include "schema.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <bits/stdc++.h>
#include <fcntl.h>

using namespace std;

int main(int argc, char ** argv) {
    if (argc < 3) {
        cout << "Usage: ./fp-analyzer <dynamic.bin> <static.bin>" << endl;
        exit(-1);
    }
    
    // Read dynamic trace result from capnp database
    cout << "Reading dynamic disassembly result from: " << argv[1] << endl;
    int fd = open(argv[1], O_RDONLY);
    
    if (fd == -1) {
        perror("Error in opening capnp binary for dynamic disassembly");
        exit(-1);
    }
    
    ::capnp::StreamFdMessageReader dynamic_message(fd);
    CaptureResult::Reader dynamic_result = dynamic_message.getRoot<CaptureResult>();
    
    const char * digest = dynamic_result.getDigest().cStr();
    int64_t base_address = dynamic_result.getBaseAddress();
    
    map<int64_t, int8_t> dynamic_offsets;
    
    for (const auto & insn : dynamic_result.getInstructions()) {
        dynamic_offsets[insn.getOffset() + base_address] = insn.getLength();
    }
    cout << "Binary digest = " << digest << endl;
    cout << "Base address = 0x" << hex << base_address << dec << endl;
    cout << "Finished reading. Total #records = " << dynamic_offsets.size() << endl;
    
    close(fd);
    
    // Read static disassembly result from capnp database
    cout << "Reading static disassembly result from: " << argv[2] << endl;
    fd = open(argv[2], O_RDONLY);
    
    if (fd == -1) {
        perror("Error in opening capnp binary for static disassembly");
        exit(-1);
    }
    ::capnp::StreamFdMessageReader static_message(fd);
    AnalysisRst::Reader static_result = static_message.getRoot<AnalysisRst>();
    
    InstOffset::Reader inst_offset = static_result.getInstOffsets();
    
    set<int64_t> static_offsets;
    for (int64_t offset : inst_offset.getOffset()) {
        static_offsets.insert(offset);
    }
    cout << "Finished reading. Total #records = " << static_offsets.size() << endl;
    
    close(fd);
    
    // Evaluation phase
    int64_t tp = 0, fp_confirm = 0, fp_other = 0, fn = 0;
    for (int64_t offset : static_offsets) {
        // *lb is >= offset
        auto lb = dynamic_offsets.lower_bound(offset);
        
        
        if (lb == dynamic_offsets.end()) {
            // offset is not in the map, and it is larger than any other offsets
            // so check with the last element
            auto elem = dynamic_offsets.rbegin();
            assert(offset > elem->first);
            if (offset < (elem->first + elem->second)) {
                ++fp_confirm;
            } else {
                ++fp_other;
            }
            // need to check this before deref in the next line
        } else if (lb->first == offset) {
            // just nice, a match is found
            ++tp;
            continue;
        } else {
            // offset is in the middle
            auto elem = prev(lb);
            assert(offset > elem->first);
            if (offset < (elem->first + elem->second)) {
                ++fp_confirm;
            } else {
                ++fp_other;
            }
        }
        
        
    }
    
    for (const auto & insn : dynamic_offsets) {
        // actually runned, but disassembler does not give it in its output
        if (static_offsets.find(insn.first) == static_offsets.end()) {
            ++fn;
        }
    }
    
    cout << "TP: " << tp << endl;
    cout << "FP: " << fp_confirm << " (confirmed), " << fp_other << " (suspected)" << endl;
    cout << "FN: " << fn << endl;
    
    return 0;
}
