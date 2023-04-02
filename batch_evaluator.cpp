#include "bits/stdc++.h"

using namespace std;

string exec(const char* cmd)
{
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

int main(int argc, char ** argv) {
    if (argc < 5) {
        cout << "Usage: ./batch_evaluator <dynamic_path> <static_path> <static_suffix> <output.csv>" << endl;
        exit(-1);
    }
    
    string dynamic_path(argv[1]),
           static_path(argv[2]),
           static_suffix(argv[3]);
    ofstream of(argv[4], ofstream::trunc);
    of << "binary,tp,fp_c,fp_o,fn" << endl;
    
    for (auto const & entry : filesystem::directory_iterator(dynamic_path)) {
        if (!entry.is_regular_file()) continue;
        
        string filename(entry.path());
        filename = filename.substr(filename.find_last_of("/\\") + 1);
        // remove last '.capnp.out' suffix
        filename = filename.substr(0, filename.size() - 10);
        
        string static_filename = static_path + "/" + filename + static_suffix;
        
        cout << "Binary: " << filename << endl;
        cout << "Static: " << static_filename << endl;
        cout << "Dynamic: " << entry.path() << endl;
        
        string cmd = "./evaluator " + string(entry.path()) + " " + static_filename;
        istringstream result(exec(cmd.c_str()));
        
        string token;
        while (!result.eof()) {
            result >> token;
            if (token == "BATCH") break;
        }
        
        int64_t tp, fp_confirm, fp_other, fn;
        result >> tp >> fp_confirm >> fp_other >> fn;
        
        of << filename << "," << tp << "," << fp_confirm << "," << fp_other << "," << fn << endl;
    }
}
