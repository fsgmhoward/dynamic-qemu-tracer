#include "schema_io.hpp"
#include <sys/stat.h>

extern "C" {
    #include <qemu-plugin.h>
    #include "elf-parser.h"
}

using namespace std;

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

// Table of instructions, which will be converted to capnp format before exit
unordered_map<int64_t, int8_t> insn_discovered;
vector<unordered_set<int64_t>> insn_executed;

// Memory mapping - begin, end, file offset, file name
unordered_set<string> filename_table;
typedef tuple<uint64_t, uint64_t, uint64_t, const string *> mapping_t;
set<mapping_t> mapping_table;
bool has_mapped = false;

const string * target_filename = nullptr;
int output_version = 0;
ofstream logger;

// Ref: https://stackoverflow.com/questions/12774207/fastest-way-to-check-if-a-file-exists-using-standard-c-c11-14-17-c
inline bool file_exists(const string& name) {
    struct stat buffer;   
    return (stat (name.c_str(), &buffer) == 0); 
}

// Ref: https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
string exec(const char* cmd) {
    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


// We *NOT ONLY* care about segments with x permission
// Note that original file is mapped with non-executable permission
// Also to ignore all files that does not match the filename
static void update_mapping()
{
    char buffer[512];
    char dispose[64];
    int pid = getpid();
    sprintf(buffer, "/proc/%d/maps", pid);
    
    ifstream m(buffer);
    
    string filename;
    char permission[5];
    uint64_t offset, inode, begin, end;
    
    while (m.getline(buffer, 512)) {
        istringstream line(buffer);
        line >> buffer >> permission >> hex >> offset >> dec >> dispose >> inode;
        if (inode != 0) {
            line >> filename;
        } else {
            continue;
        }
        
        const string * filename_ptr = &(*filename_table.insert(filename).first);
        if (filename_ptr != target_filename) {
            continue;
        }
        
        // convert range to integer
        char * delim = strchr(buffer, '-');
        *delim = 0;
        begin = stoull(buffer, nullptr, 16);
        end = stoull(delim + 1, nullptr, 16);
        
        mapping_table.emplace(begin, end, offset, filename_ptr);
    }
    
    has_mapped = true;
}

/*
 * For the sake of efficiency, we assumed that mapping stays unchanged
 * throughout execution.
 *
 * Inputs virtual address, outputs offset
 *
 * This should always succeed - it should not enter infinite loop
 */
static int64_t resolve_mapping(uint64_t vaddr)
{
    if (!has_mapped) {
        update_mapping();
    }
    for (const mapping_t & mapping : mapping_table)
    {
        // no suitable range found
        if (vaddr < get<0>(mapping)) {
            break;
        }
        if (vaddr >= get<1>(mapping)) {
            continue;
        }
        // now: begin <= vaddr < end
        // vaddr - begin + base
        int64_t offset = vaddr - get<0>(mapping) + get<2>(mapping);
        return offset;
    }
    
    return -1;
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    if (userdata) {
        insn_executed[vcpu_index].insert((int64_t) userdata);
    }
}

/**
 * On translation block new translation
 *
 * QEMU convert code by translation block (TB). By hooking here we can then hook
 * a callback on each instruction and memory access.
 *
 * One TB might contain multiple instructions
 * Note: not all instructions translated might be executed - so hook is set with instruction callback
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;
    
    int n = qemu_plugin_tb_n_insns(tb);
    for (int i = 0; i < n; ++i) {
        insn = qemu_plugin_tb_get_insn(tb, i);
        
        uint64_t vaddr = qemu_plugin_insn_vaddr(insn);
        int64_t offset = resolve_mapping(vaddr);
        
        // void * aka uint64_t, assuming there is no instruction sitting at address 0
        void * insn_data = nullptr;
        // Store only instructions that match with the static file
        if (offset != -1) {
            uint8_t length = (uint8_t) qemu_plugin_insn_size(insn);
            
            // filename, offset, length
            insn_data = (void*) offset;
            insn_discovered[offset] = length;
        }
        
        /* Register callback on instruction */
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec, QEMU_PLUGIN_CB_NO_REGS, insn_data);
    }
}

// This function parses target's ELF header and find out the image loading base address
static int64_t parse_base_address()
{
    // Resolve section offset from ELF header
    // Ref: https://github.com/TheCodeArtist/elf-parser/blob/master/elf-parser-main.c
    int elffd = open(target_filename->c_str(), O_RDONLY|O_SYNC);
    Elf32_Ehdr eh;
    read_elf_header(elffd, &eh);
    
    // base address where file offsets will add up to it
    int64_t base_address = 0;
    
    if(is64Bit(eh)){
		Elf64_Ehdr eh64;	/* elf-header is fixed size */
		Elf64_Shdr* sh_tbl;	/* section-header table is variable size */

		read_elf_header64(elffd, &eh64);

		/* Section header table :  */
		sh_tbl = (Elf64_Shdr*) malloc(eh64.e_shentsize * eh64.e_shnum);
		read_section_header_table64(elffd, eh64, sh_tbl);
		
		for (int i = 0; i < eh64.e_shnum; ++i) {
		    int flag = sh_tbl[i].sh_flags;
		    // 0x2: SHF_ALLOC
		    // 0x4: SHF_EXECINSTR
		    // See: https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/6n33n7fcj/index.html
		    if ((flag & 0x2) && (flag & 0x4)) {
		        base_address = sh_tbl[i].sh_addr - sh_tbl[i].sh_offset;
		        break;
		    }
		}
		free(sh_tbl);
	} else{
		Elf32_Shdr* sh_tbl = (Elf32_Shdr*) malloc(eh.e_shentsize * eh.e_shnum);
		read_section_header_table(elffd, eh, sh_tbl);
		
		for (int i = 0; i < eh.e_shnum; ++i) {
		    int flag = sh_tbl[i].sh_flags;
		    // 0x2: SHF_ALLOC
		    // 0x4: SHF_EXECINSTR
		    // See: https://docs.oracle.com/cd/E19120-01/open.solaris/819-0690/6n33n7fcj/index.html
		    if ((flag & 0x2) && (flag & 0x4)) {
		        base_address = sh_tbl[i].sh_addr - sh_tbl[i].sh_offset;
		        break;
		    }
		}
		free(sh_tbl);
	}
	
	close(elffd);
	
	logger << "Base Address parsed @0x" << hex << base_address << dec << endl;
	
	return base_address;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    // Merge recorded insn and sort them accordingly
    map<int64_t, int8_t> instructions;
    for (size_t i = 0; i < insn_executed.size(); ++i) {
        for (int64_t offset : insn_executed[i]) {
            instructions.emplace(offset, insn_discovered[offset]);
        }
    }
    
    // Save file is base name + .capnp.out
	string output(strrchr(target_filename->c_str(), '/') + 1);
	output += ".capnp.out";
	logger << "Saving output to " << output << endl;
    
    int64_t base_address = -1;
    if (output_version == 0) {
        base_address = parse_base_address();
        if (!write_version_0(output.c_str(), instructions, base_address)) {
            logger << "Failed to write to output file" << endl;
        }
    } else {
        // Calculate executable digest
        string digest;
        string command("md5sum -b ");
        command += target_filename->c_str();
        istringstream exec_out(exec(command.c_str()));
        exec_out >> digest;
        logger << "Executable MD5 digest: " << digest << endl;
        
        if (file_exists(output.c_str())) {
            logger << "Output exists. Loading it." << endl;
            // base_address = input_version_1(instructions, output.c_str(), digest);
            string original_digest;
            map<int64_t, int8_t> original_instructions;
            if (!read_version_1(output.c_str(), original_instructions, base_address, original_digest)) {
                logger << "Failed to read from input file" << endl;
                base_address = -1;
            } else if (original_digest != digest) {
                logger << "Digest mismatch, probably due to a recompilation of the file" << endl;
                logger << "Original output digest: " << original_digest << endl;
                logger << "Original output will be disposed" << endl;
                base_address = -1;
            } else {
                logger << "Original output digest match" << endl;
                logger << "Merging two sets of instructions:" << endl;
                logger << "#Insns captured in this run = " << instructions.size() << endl;
                logger << "#Insns in the old capture file = " << original_instructions.size() << endl;
                instructions.insert(original_instructions.begin(), original_instructions.end());
                logger << "Finished merging" << endl;
                logger << "#Insns after merging two sets = " << instructions.size() << endl;
            }
        }
        if (base_address == -1) {
            base_address = parse_base_address();
        }
        if (!write_version_1(output.c_str(), instructions, base_address, digest)) {
            logger << "Failed to write to output file" << endl;
        }
    }
    
    logger.close();
}

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    /*
     * Initialize dynamic array to cache vCPU instruction.
     * This could be huge and is not limited to thread::hardware_concurrency() - QEMU spawns 1 vcpu per
     * thread, regardless of how many CPUs the host has.
     * FIXME: Set to 255 as a temporary resolution
     */
    insn_executed.resize(255);
    
    if (argc == 0) {
        cerr << "Expect at least 1 argument 'binary=<binary_file_location>'\n";
        return -1;
    }
    
    logger.open("capnp-capture.log", ios::out | ios::app);
    
    // argv[0]: binary=<binary_file>
    // argv[1]: version=0 (default, backward compatible with gt generator)
    //                  1 (new, stores file md5 information, etc.)
    // argv[2]: mode=0 (default, output capnp-serialized instruction offset table)
    //               1 (output .txt format of human-readable disassembly result)
    //               2 (output qemu translation block addresses (not just those instructions being actually translated)
    char * filename = strchr(argv[0], '=') + 1;
    // DEBUG
    logger << "Tracing " << filename << endl;
    auto [it, n] = filename_table.emplace(filename);
    target_filename = &*it;
    
    if (argc > 1) {
        output_version = atoi(strchr(argv[1], '=') + 1);
    }
    logger << "Output Version " << output_version << endl;

    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
