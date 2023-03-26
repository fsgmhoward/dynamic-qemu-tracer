#include "schema_reader.hpp"

namespace std {   
    static int open_file(const char * file) {
        DEBUG_PRINT("Reading capnproto serialized file from: " << argv[1]);
        int fd = open(file, O_RDONLY);
        if (fd == -1) {
            perror("Error in opening capnproto serialized file");
        }
        return fd;
    }
    
    bool read_version_0(
        const char * file,
        set<int64_t> & offsets
    ) {
        int fd = open_file(file);
        if (fd == -1) return false;
        
        ::capnp::StreamFdMessageReader static_message(fd);
        auto static_result = static_message.getRoot<AnalysisRst>();
        auto inst_offset = static_result.getInstOffsets();
        
        for (int64_t offset : inst_offset.getOffset()) {
            offsets.insert(offset);
        }
        
        close(fd);
        return true;
    }
    
    bool read_version_1(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t & base_address,
        string & digest
    ) {
        int fd = open_file(file);
        if (fd == -1) return false;
        
        ::capnp::StreamFdMessageReader dynamic_message(fd);
        auto dynamic_result = dynamic_message.getRoot<CaptureResult>();
        
        digest = dynamic_result.getDigest().cStr();
        base_address = dynamic_result.getBaseAddress();
        
        for (const auto & insn : dynamic_result.getInstructions()) {
            instructions[insn.getOffset() + base_address] = insn.getLength();
        }
        
        close(fd);
        return true;
    }
}
