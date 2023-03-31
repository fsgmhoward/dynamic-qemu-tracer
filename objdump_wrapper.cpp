#include "schema_io.hpp"

using namespace std;

#define LST_SUFFIX "_objdump_raw.txt"
#define CAPNP_SUFFIX "_objdump.out"

// Ref: https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
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

inline int countchar(string haystack, char needle)
{
    return count(haystack.begin(), haystack.end(), needle);
}

int main(int argc, char ** argv)
{
    if (argc < 2) {
        cerr << "./objdump_wrapper <input_folder> <output_folder>" << endl;
        exit(-1);
    }
    
    string inpath(argv[1]), outpath(argv[2]);
    
    for (const auto & entry : filesystem::directory_iterator(inpath)) {
        if (!entry.is_regular_file()) continue;
        
        cout << "Processing " << entry.path() << endl;
        
        string filename(entry.path());
        filename = filename.substr(filename.find_last_of("/\\") + 1);
        string lst_outpath = outpath + "/" + filename + LST_SUFFIX;
        
        cout << "Raw LST output: " << lst_outpath << endl;
        
        string cmd("objdump -d ");
        cmd += string(entry.path()) + " > " + lst_outpath;
        exec(cmd.c_str());
        
        ifstream lst(lst_outpath);
        string line;
        
        map<int64_t, int8_t> instructions;
        int error_count = 0;
        
        while (getline(lst, line)) {
            if (countchar(line, ':') != 0 && line[0] == ' ') {
                line[line.find(':')] = ' ';
                istringstream iss(line);
                int64_t addr;
                iss >> hex >> addr;
                if (countchar(line, '\t') != 2) {
                    // only 1 tab, meaning objdump fails to decode this offset
                    // cerr << "Objdump fails to decode at: 0x" << hex << addr << dec << endl;
                    ++error_count;
                } else {
                    instructions.emplace(addr, 0);
                }
            }
        }
        
        string capnp_outpath = outpath + "/" + filename + CAPNP_SUFFIX;
        cout << "Capnp output: " << capnp_outpath << endl;
        write_version_0(capnp_outpath.c_str(), instructions, 0);
        
        cout << "Finished. #errors = " << error_count << endl;
    }

}
