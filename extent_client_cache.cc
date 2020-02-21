#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "extent_client_cache.h"

extent_client_cache::extent_client_cache(std::string dst) : extent_client(dst),of("extent_out.txt",std::ios_base::app)
{
}

// a demo to show how to use RPC
extent_protocol::status
extent_client_cache::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
    ret = cl->call(extent_protocol::create, type, id);
     extent_cache_table[id].attr.atime=time(0);
    extent_cache_table[id].m_state = NONE;
     extent_cache_table[id].attr.type=type;
    extent_cache_table[id].attr.size =0;
  return ret;
}


extent_protocol::status
extent_client_cache::get(extent_protocol::extentid_t eid, std::string &buf)
{
    extent_protocol::status ret = extent_protocol::OK;

of<<"get"<<eid;
    std::lock_guard<std::mutex> lg(mutex);

    if(extent_cache_table.count(eid))
    {
        switch (extent_cache_table[eid].m_state)
        {
            case UPDATE:
            case MODIFIED:
                // new
                buf = extent_cache_table[eid].data;
                extent_cache_table[eid].attr.atime = time(0);
                break;

            case NONE:
                ret = cl->call(extent_protocol::get, eid, buf);
                // get from server
                if(ret == extent_protocol::OK)
                {
                    extent_cache_table[eid].data = buf;
                    extent_cache_table[eid].m_state = UPDATE;
                    extent_cache_table[eid].attr.atime = time(0);
                    extent_cache_table[eid].attr.size = buf.size();
                }
                break;
            case REMOVED:
                return  extent_protocol::NOENT;
        }
    }
    // no cache
    else
    {
        ret = cl->call(extent_protocol::get, eid, buf);
        extent_protocol::attr attr;
        ret = cl->call(extent_protocol::getattr, eid, attr);
        if (ret == extent_protocol::OK)
        {
            extent_cache_table[eid].data = buf;
            extent_cache_table[eid].m_state = UPDATE;
            extent_cache_table[eid].attr = attr;
            extent_cache_table[eid].attr.atime = time(0);
        }
    }

    return ret;
}

extent_protocol::status
extent_client_cache::getattr(extent_protocol::extentid_t eid,
                             extent_protocol::attr &attr)
{
    extent_protocol::status ret = extent_protocol::OK;

of<<"attr"<<eid;
    std::lock_guard<std::mutex> lg(mutex);

    if(extent_cache_table.count(eid))
    {
        switch (extent_cache_table[eid].m_state)
        {
            case UPDATE:
            case MODIFIED:
            case NONE:
                attr = extent_cache_table[eid].attr;
of<<eid<<" type :"<<attr.type;
                break;

            case REMOVED:
                return extent_protocol::NOENT;
        }
    }
    else
    {
        ret = cl->call(extent_protocol::getattr, eid, attr);
        if(ret == extent_protocol::OK){
            extent_cache_table[eid].attr= attr;
        extent_cache_table[eid].m_state = NONE;
        }

    }

    return ret;
}

extent_protocol::status
extent_client_cache::put(extent_protocol::extentid_t eid, std::string buf)
{
    extent_protocol::status ret = extent_protocol::OK;
of<<"put"<<eid;
    std::lock_guard<std::mutex> lg(mutex);

    if(extent_cache_table.count(eid))
    {
        switch (extent_cache_table[eid].m_state)
        {
            case NONE:
            case UPDATE:
            case MODIFIED:
                break;

            case REMOVED:
                return extent_protocol::NOENT;
        }
    }
        extent_cache_table[eid].data = buf;
        extent_cache_table[eid].m_state = MODIFIED;
        extent_cache_table[eid].attr.mtime = time(0);
        extent_cache_table[eid].attr.ctime = time(0);
        extent_cache_table[eid].attr.size = buf.size();
    return ret;
}

extent_protocol::status
extent_client_cache::remove(extent_protocol::extentid_t eid)
{
    extent_protocol::status ret = extent_protocol::OK;

    std::lock_guard<std::mutex> lg(mutex);

    if (extent_cache_table.count(eid))
    {
        switch (extent_cache_table[eid].m_state)
        {
        case NONE:
        case UPDATE:
        case MODIFIED:
            break;

        case REMOVED:
            return extent_protocol::NOENT;
        }
    }
        extent_cache_table[eid].m_state = REMOVED;

    return ret;
}

extent_protocol::status
extent_client_cache::flush(extent_protocol::extentid_t eid)
{
    extent_protocol::status ret = extent_protocol::OK;
    int r;
of<<"flush"<<eid;

    std::lock_guard<std::mutex> lg(mutex);

    if(extent_cache_table.count(eid))
    {
        switch (extent_cache_table[eid].m_state)
        {
            case MODIFIED:
                ret = cl->call(extent_protocol::put, eid, extent_cache_table[eid].data, r);
                break;

            case REMOVED:
                ret = cl->call(extent_protocol::remove, eid);
                break;
            
            case NONE:
            case UPDATE:
                break;
        }
        extent_cache_table.erase(eid);
    }
    else
        return  extent_protocol::NOENT;

    return ret;
}
