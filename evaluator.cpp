/**
 * This program evaluates the static disassembly result against dynamic disassembly (traced) result
 * By default, static disassembly uses version 0 output and dynamic one uses version 1 output
 */
// Example: ./evaluator ~/Desktop/TestOutput/dynamic/cpugcc_r_base.mytest-m64.orig.capnp.out ~/Desktop/TestOutput/static/stripall/ghidra/cpugcc_r_base.mytest-m64.orig_ghidra.out

#define _DEBUG_

#include "schema_reader.hpp"

using namespace std;

int main(int argc, char ** argv) {
    if (argc < 3) {
        cout << "Usage: ./evaluator <dynamic.bin> <static.bin>" << endl;
        exit(-1);
    }
    
    // Read dynamic trace result from capnp database
    map<int64_t, int8_t> dynamic_offsets;
    int64_t base_address;
    string digest;
    
    read_version_1(argv[1], dynamic_offsets, base_address, digest);
    
    cout << "Binary digest = " << digest << endl;
    cout << "Base address = 0x" << hex << base_address << dec << endl;
    cout << "Finished reading. Total #records = " << dynamic_offsets.size() << endl;
    
    // Read static disassembly result from capnp database
    cout << "Reading static disassembly result from: " << argv[2] << endl;
    
    set<int64_t> static_offsets;
    read_version_0(argv[2], static_offsets);
    
    cout << "Finished reading. Total #records = " << static_offsets.size() << endl;
    
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
