/**
 * This program evaluates the static disassembly result against dynamic disassembly (traced) result
 * By default, static disassembly uses version 0 output and dynamic one uses version 1 output
 */
// Example: ./evaluator ~/Desktop/TestOutput/gcc-11-O3/dynamic/perlbench_r_base.mytest-m64.orig.capnp.out ~/Desktop/TestOutput/gcc-11-O3/static/stripall/ghidra-10.2.3/perlbench_r_base.mytest-m64.orig_ghidra.out fplist.txt fnlist.txt

#define _DEBUG_

#include "schema_io.hpp"

using namespace std;

int main(int argc, char ** argv) {
    if (argc < 3) {
        cout << "Usage: ./evaluator <dynamic.bin> <static.bin> [fplist.txt] [fnlist.txt] [unklist.txt]" << endl;
        exit(-1);
    }
    
    bool is_fplist_enabled;
    bool is_fnlist_enabled = false;
    bool is_unklist_enabled = false;
    ofstream fplist, fnlist, unklist;
    if (is_fplist_enabled = (argc > 3)) {
        fplist.open(argv[3], ofstream::trunc);
        fplist << hex;
        cout << "Confirmed FPs will be written to this file: " << argv[3] << endl;
        
        if (is_fnlist_enabled = (argc > 4)) {
            fnlist.open(argv[4], ofstream::trunc);
            fnlist << hex;
            cout << "FNs will be written to this file: " << argv[4] << endl;
            cout << "Warning: FN list might be VERY long" << endl;
            
            if (is_unklist_enabled = (argc > 5)) {
                unklist.open(argv[5], ofstream::trunc);
                unklist << hex;
                cout << "UNKs will be written to this file: " << argv[5] << endl;
                cout << "Warning: UNK list might be VERY long" << endl;
            }
        }
    }
    
    // Read dynamic trace result from capnp database
    map<int64_t, int8_t> dynamic_offsets;
    int64_t base_address;
    string digest;
    
    // Note: dynamic_offsets here exclude base_address
    read_version_1(argv[1], dynamic_offsets, base_address, digest);
    
    cout << "Binary digest = " << digest << endl;
    cout << "Base address = 0x" << hex << base_address << dec << endl;
    cout << "Finished reading. Total #records = " << dynamic_offsets.size() << endl;
    
    // Read static disassembly result from capnp database
    cout << "Reading static disassembly result from: " << argv[2] << endl;
    
    set<int64_t> static_offsets;
    // Note: static_offsets here include base_address
    read_version_0(argv[2], static_offsets);
    
    cout << "Finished reading. Total #records = " << static_offsets.size() << endl;
    
    // Evaluation phase
    int64_t tp = 0, fp = 0, unk = 0, fn = 0;
    for (int64_t offset : static_offsets) {
        // Exclude base_address
        offset -= base_address;
        // *lb is >= offset
        auto lb = dynamic_offsets.lower_bound(offset);
        
        
        if (lb == dynamic_offsets.end()) {
            // offset is not in the map, and it is larger than any other offsets
            // so check with the last element
            auto elem = dynamic_offsets.rbegin();
            assert(offset > elem->first);
            if (offset < (elem->first + elem->second)) {
                ++fp;
                if (is_fplist_enabled) {
                    fplist << "E: " << elem->first << " L: " << elem->second << " A: " << offset << endl;
                }
            } else {
                ++unk;
                if (is_unklist_enabled) {
                    unklist << offset << endl;
                }
            }
            // need to check this before deref in the next line
        } else if (lb->first == offset) {
            // just nice, a match is found
            ++tp;
            continue;
        } else if (lb == dynamic_offsets.begin()) {
            // offset is smaller than any instructions in the set
            ++unk;
            if (is_unklist_enabled) {
                unklist << offset << endl;
            }
        } else {
            // offset is in the middle
            auto elem = prev(lb);
            assert(offset > elem->first);
            if (offset < (elem->first + elem->second)) {
                ++fp;
                if (is_fplist_enabled) {
                    // Expected offset, expected length, actual offset disassembled
                    fplist << "E: " << elem->first << " L: " << (int) elem->second << " A: " << offset << endl;
                }
            } else {
                ++unk;
                if (is_unklist_enabled) {
                    unklist << offset << endl;
                }
            }
        }
    }
    
    for (const auto & insn : dynamic_offsets) {
        // actually runned, but disassembler does not give it in its output
        if (static_offsets.find(insn.first + base_address) == static_offsets.end()) {
            ++fn;
            fnlist << insn.first << endl;
        }
    }
    
    cout << "TP: " << tp << endl;
    cout << "FP: " << fp << endl;
    cout << "UNK: " << unk << endl;
    cout << "FN: " << fn << endl;
    
    // Output one more time for batch reader
    cout << "BATCH " << tp << " " << fp << " " << unk << " " << fn << endl;
    
    if (is_fplist_enabled) {
        fplist.close();
        if (is_fnlist_enabled) {
            fnlist.close();
            if (is_unklist_enabled) {
                unklist.close();
            }
        }
    }
    
    return 0;
}
