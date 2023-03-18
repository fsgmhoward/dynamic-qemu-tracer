#include "schema.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <bits/stdc++.h>
#include <fcntl.h>

// ./fp-analyzer ~/GTSource/output/x86_64-pc-linux-gnu-clang-6.0.0/%2dO3/gt/spec2017/leela_r.sqlite ~/GTSource/output/x86_64-pc-linux-gnu-clang-6.0.0/%2dO3/radare2/spec2017/mcf_r_r2.out

extern "C" {
    #include <sqlite3.h>
}

using namespace std;

// Ref: https://www.runoob.com/sqlite/sqlite-c-cpp.html

// Executed for every row
static int callback(void *data, int argc, char **argv, char **azColName){
    set<int64_t> * gt = (set<int64_t>*) data;
    gt->insert(atoll(argv[0]));
    return 0;
}

int main(int argc, char ** argv) {
    if (argc < 3) {
        cout << "Usage: ./fp-analyzer <gt.sqlite> <output.bin> [binary_file]" << endl;
        exit(-1);
    }
    
    sqlite3 * db;
    int rc = sqlite3_open(argv[1], &db);
    char * zErrMsg;
    
    if( rc ){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(0);
    }else{
        fprintf(stderr, "Opened database successfully\n");
    }
    
    
    /* Create SQL statement */
    const char * sql = "SELECT offset from insn";
    
    set<int64_t> gt;

    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, callback, (void*)&gt, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        exit(-1);
    }else{
        fprintf(stdout, "Read from ground truth database done successfully\n");
        
        // for (int64_t x : gt) {
        //     cout << hex << x << endl;
        // }
    }
    sqlite3_close(db);
    
    // Read from capnp database
    cout << "Reading capnp result from: " << argv[2] << endl;
    int fd = open(argv[2], O_RDONLY);
    
    if (fd == -1) {
        perror("Error in opening capnp binary");
        exit(-1);
    }
    ::capnp::StreamFdMessageReader message(fd);
    AnalysisRst::Reader rst = message.getRoot<AnalysisRst>();
    
    InstOffset::Reader inst_offset = rst.getInstOffsets();
    
    int tp_count = 0, fp_count = 0, fn_count = 0;
    for (int addr : inst_offset.getOffset()) {
        if (gt.erase(addr)) {
            tp_count += 1;
        } else {
            fp_count += 1;
            cout << "FP @ " << hex << addr << endl;
        }
    }
    fn_count = gt.size();
    cout << "TP, FP, FN count = " << dec << tp_count << ", " << fp_count << ", " << fn_count << endl;
    
    return 0;
}
