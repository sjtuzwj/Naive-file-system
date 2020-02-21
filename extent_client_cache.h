#ifndef extent_client_cache_h
#define extent_client_cache_h

#include <mutex>
#include "extent_client.h"
#include <fstream>

class extent_client_cache : public extent_client {
    enum state {
        NONE,       // only attr
        UPDATE,     // new
        MODIFIED,   // modified
        REMOVED      // delete
    };

    struct extent {
        std::string data;
        state m_state;
        extent_protocol::attr attr;
        extent() : m_state(NONE) {}
    };

private:
    std::mutex mutex;
std::ofstream of;
    std::map<extent_protocol::extentid_t, extent> extent_cache_table;

public:
    extent_client_cache(std::string dst);
    extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
    extent_protocol::status get(extent_protocol::extentid_t eid,
                                std::string &buf);
    extent_protocol::status getattr(extent_protocol::extentid_t eid,
                                    extent_protocol::attr &a);
    extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
    extent_protocol::status remove(extent_protocol::extentid_t eid);
    extent_protocol::status flush(extent_protocol::extentid_t eid);
};

#endif
