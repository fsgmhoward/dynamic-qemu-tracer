#include "schema_io.hpp"

namespace std {   
    static int open_file(const char * file, bool is_writing = false) {
        DEBUG_PRINT("Reading capnproto serialized file from: " << argv[1]);
        int fd;
        if (is_writing) {
            fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        } else {
            fd = open(file, O_RDONLY);
        }
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
    
    // Base address will be added to all entries written
    // This is for compatibility purpose
    bool write_version_0(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t base_address = 0
    ) {
        int fd = open_file(file, true);
        if (fd == -1) return false;
        
        ::capnp::MallocMessageBuilder message;
        auto analysis_rst = message.initRoot<AnalysisRst>();
        auto inst_offsets = analysis_rst.initInstOffsets();
        auto offsets = inst_offsets.initOffset(instructions.size());
        
        size_t i = 0;
        for (const auto & insn : instructions) {
            offsets.set(i, insn.first + base_address);
            ++i;
        }
        
        writeMessageToFd(fd, message);
        
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
            instructions[insn.getOffset()] = insn.getLength();
        }
        
        close(fd);
        return true;
    }
    
    bool write_version_1(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t base_address,
        string & digest
    ) {
        // Create output file, with 644 permission
        int fd = open_file(file, true);
        if (fd == -1) return false;
        
        // Write to capnp binary output
        ::capnp::MallocMessageBuilder message;
        auto result = message.initRoot<CaptureResult>();
        result.setDigest(digest.c_str());
        result.setBaseAddress(base_address);
        
        auto output_insns = result.initInstructions(instructions.size());
        
        size_t i = 0;
        for (const auto & insn : instructions) {
            output_insns[i].setOffset(insn.first);
            output_insns[i].setLength(insn.second);
            ++i;
        }
        
        writeMessageToFd(fd, message);
        
        close(fd);
        return true;
    }
}
