#ifndef _SCHEMA_READER_HPP_
#define _SCHEMA_READER_HPP_

#include "schema.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <bits/stdc++.h>
#include <fcntl.h>

#ifdef _DEBUG_
    #define DEBUG_PRINT(msg) std::cerr << msg << std::endl;
#else
    #define DEBUG_PRINT(msg) {}
#endif

namespace std {
    // Return value represents whether the read is successful
    // NOTE: For all instructions input/output, it should EXCLUDE base_address
    // NOTE2: For all 'offsets', it INCLUDE base_address (because V0 input does not have a base_address field)
    bool read_version_0(
        const char * file,
        set<int64_t> & offsets
    );
    bool write_version_0(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t base_address
    );
    bool read_version_1(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t & base_address,
        string & digest
    );
    bool write_version_1(
        const char * file,
        map<int64_t, int8_t> & instructions,
        int64_t base_address,
        string & digest
    );
}

#endif
