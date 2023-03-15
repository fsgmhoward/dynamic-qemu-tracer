#include "schema.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <bits/stdc++.h>
#include <fcntl.h>

extern "C" {
    #include <qemu-plugin.h>
    #include "elf-parser.h"
}

using namespace std;

// struct insn_t {
//     const string * filename;
//     uint64_t offset;
//     uint8_t length;
//     
//     insn_t(const string * f, uint64_t o, uint8_t l) : filename(f), offset(o), length(l) {}
// };
typedef pair<uint64_t, uint8_t> insn_t;

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

// Table of instructions, represented as insn_t as its size is unknown
// It will be converted to capnp format before exit
vector<set<insn_t>> insn_table;

// Memory mapping - begin, end, file offset, file name
unordered_set<string> filename_table;
typedef tuple<uint64_t, uint64_t, uint64_t, const string *> mapping_t;
set<mapping_t> mapping_table;

const string * target_filename = nullptr;

// We *NOT ONLY* care about segments with x permission
// Note that original file is mapped with non-executable permission
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
            filename = "[runtime]";
        }
        
        // convert range to integer
        char * delim = strchr(buffer, '-');
        *delim = 0;
        begin = stoull(buffer, nullptr, 16);
        end = stoull(delim + 1, nullptr, 16);
        
        const string * filename_ptr = &(*filename_table.insert(filename).first);
        mapping_table.emplace(begin, end, offset, filename_ptr);
    }
}

/*
 * For the sake of efficiency, we assumed that mapping stays unchanged
 * throughout execution.
 *
 * Inputs virtual address, outputs offset & files
 *
 * This should always succeed - it should not enter infinite loop
 */
static pair<uint64_t, const string *> resolve_mapping(uint64_t vaddr)
{
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
        uint64_t offset = vaddr - get<0>(mapping) + get<2>(mapping);
        return make_pair(offset, get<3>(mapping));
    }
    update_mapping();
    return resolve_mapping(vaddr);
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    if (userdata) {
        tuple<const string *, uint64_t, uint8_t> * insn = (tuple<const string *, uint64_t, uint8_t> *) userdata;
    
        insn_table[vcpu_index].emplace(get<1>(*insn), get<2>(*insn));
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
        auto [offset, filename] = resolve_mapping(vaddr);
        
        tuple<const string *, uint64_t, uint8_t> * insn_data = nullptr;
        // Store only instructions that match with the static file
        if (filename == target_filename) {
            uint8_t length = (uint8_t) qemu_plugin_insn_size(insn);
            
            // filename, offset, length
            insn_data = new tuple<const string *, uint64_t, uint8_t>(filename, offset, length);
        }
        
        // TODO: Free contents
        
        /* Register callback on instruction */
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec, QEMU_PLUGIN_CB_NO_REGS, insn_data);
    }
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
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

    // Create output file, with 644 permission
    int fd = open("execlog_capnp.out", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    // char buf[100];
    // fprintf(fd, "\n ========= BEGIN MAPPING ========= \n");
    // sprintf(buf, "/proc/%d/maps", getpid());
    // FILE * mapfd = fopen(buf, "r");
    // do {
    //     fgets(buf, 100, mapfd);
    //     fputs(buf, fd);
    // } while (!feof(mapfd));
    // fclose(mapfd);
    // fprintf(fd, "\n ========== END MAPPING ========== \n");
    
    // Merge recorded insn from other CPUs into the set for CPU 0
    for (size_t i = 1; i < insn_table.size(); ++i) {
        insn_table[0].insert(insn_table[i].begin(), insn_table[i].end());
    }
    
    // Write to capnp binary output
    ::capnp::MallocMessageBuilder message;
    AnalysisRst::Builder analysis_rst = message.initRoot<AnalysisRst>();
    
    InstOffset::Builder inst_offsets = analysis_rst.initInstOffsets();
    ::capnp::List<int64_t>::Builder offsets = inst_offsets.initOffset(insn_table[0].size());
    
    size_t i = 0;
    for (const insn_t & insn : insn_table[0]) {
        offsets.set(i, insn.first + base_address);
        ++i;
    }
    
    writeMessageToFd(fd, message);
    
    close(fd);
    
    // cerr << "Executable Memory Regions" << endl;
    // for (const mapping_t & mapping : mapping_table)
    // {
    //     cerr << get<0>(mapping) << " -> " << get<1>(mapping) << " : " << *get<3>(mapping) << "@" << get<2>(mapping) << endl;
    // }
}

/**
 * Install the plugin
 */
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info, int argc,
                                           char **argv)
{
    /*
     * Initialize dynamic array to cache vCPU instruction. In user mode
     * we don't know the size before emulation.
     */
    insn_table.resize(thread::hardware_concurrency());
    
    if (argc != 1) {
        cerr << "Expect 1 argument 'binary=<binary_file_location>'\n";
        return -1;
    }
    
    char * filename = strchr(argv[0], '=') + 1;
    // DEBUG
    cerr << "Tracing " << filename << "\n";
    auto [it, n] = filename_table.emplace(filename);
    target_filename = &*it;

    /* Register translation block and exit callbacks */
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
