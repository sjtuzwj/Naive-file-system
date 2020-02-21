// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

pthread_mutex_t lock_server_cache::mutex = PTHREAD_MUTEX_INITIALIZER;

lock_server_cache::lock_server_cache()
{
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int &)
{
  pthread_mutex_lock(&mutex);
  lock_protocol::status ret = lock_protocol::OK; 
  bool needRevoke = false;
  
  if (!lock_cache_status_table.count(lid)){
  	lock_cache_status_table[lid].clientId = id;
    lock_cache_status_table[lid].status = LOCKED;
  }
  else switch(lock_cache_status_table[lid].status){
      //直接获取
      case FREE:
  		  lock_cache_status_table[lid].status = LOCKED;
  		  lock_cache_status_table[lid].clientId = id;
        break;
      //进入等待队列
      case LOCKED:
  		  lock_cache_status_table[lid].status = WAITING;
  		  lock_cache_status_table[lid].clientQ.push(id);
        lock_cache_status_table[lid].clientS.insert(id);
  		  ret = lock_protocol::RETRY;
  		  needRevoke = true;
        break;
      case WAITING:
      //若为队首
        if(lock_cache_status_table[lid].clientQ.front() == id){
          lock_cache_status_table[lid].clientId = id;
          lock_cache_status_table[lid].clientQ.pop();
          lock_cache_status_table[lid].clientS.erase(
          lock_cache_status_table[lid].clientS.find(id));
          if(lock_cache_status_table[lid].clientQ.empty()){
              lock_cache_status_table[lid].status = LOCKED;
          }
          else
              needRevoke = true;
        }
      //若不存在于队列加入队尾
        else{
  				ret = lock_protocol::RETRY;
          if(!lock_cache_status_table[lid].clientS.count(id)){
  				lock_cache_status_table[lid].clientQ.push(id);
          lock_cache_status_table[lid].clientS.insert(id);
          }
        }
        break;
      default:
        break;
    }
  if (needRevoke){
  	handle h(lock_cache_status_table[lid].clientId);
  	rpcc *cl = h.safebind();
  	if(cl){
  		pthread_mutex_unlock(&mutex);
      int r;
  		cl->call(rlock_protocol::revoke, lid, r);
  		pthread_mutex_lock(&mutex);
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
  pthread_mutex_lock(&mutex);
  lock_protocol::status ret = lock_protocol::OK;
  bool needRetry = false;
  if (!lock_cache_status_table.count(lid));
  else if(lock_cache_status_table[lid].clientId!=id);
  else{
    switch(lock_cache_status_table[lid].status){
      case FREE:
        pthread_mutex_unlock(&mutex);
        return ret;
      case LOCKED:
  	    lock_cache_status_table[lid].status = FREE;
  		  lock_cache_status_table[lid].clientId = "";
        break;
      case WAITING:
  		  lock_cache_status_table[lid].clientId = "";
        needRetry = true;
        break;
      default:
        break;
    }
  }
  if (needRetry)
  {
  	handle h(
       lock_cache_status_table[lid].clientQ.empty()? "":
      lock_cache_status_table[lid].clientQ.front());
  	rpcc *cl = h.safebind();
  	if(cl){
  		pthread_mutex_unlock(&mutex);
      int r;
  		cl->call(rlock_protocol::retry, lid, r);
  		pthread_mutex_lock(&mutex);
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  lock_protocol::status ret = lock_protocol::OK;
  return ret;
}

