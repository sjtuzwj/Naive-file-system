// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

//pthread _mutex_lock(&mutex)
//while或if(线程执行的条件是否成立)
//pthread_cond_wait(&cond, &mutex);
//线程执行
//pthread_mutex_unlock(&mutex);
pthread_mutex_t lock_client_cache::mutex = PTHREAD_MUTEX_INITIALIZER;
int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu),of("lock_out.txt")
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&mutex);
  int ret = lock_protocol::OK;
  bool acquire = false;

  if (!lock_cache_status_table.count(lid)){
    acquire = true;
    lock_cache_status_table[lid];
  }
  recheck:
    switch(lock_cache_status_table[lid].status){
      case NONE:
        acquire = true;
        lock_cache_status_table[lid].status = ACQUIRING;
        break;
      case FREE:
        lock_cache_status_table[lid].status = LOCKED;
        break;
      case LOCKED:
      case ACQUIRING:
      case RELEASING:  
        //等待条件成立后线程执行再次获取锁
        while 
        (lock_cache_status_table[lid].status == LOCKED ||
         lock_cache_status_table[lid].status == ACQUIRING || 
         lock_cache_status_table[lid].status == RELEASING)
        pthread_cond_wait(&lock_cache_status_table[lid].cond, &mutex);
        goto recheck;
        break;
      default:
        break;
  }
  if (acquire){
    while (!lock_cache_status_table[lid].retry){
      //请求server
      int r;
      pthread_mutex_unlock(&mutex);
      ret = cl->call(lock_protocol::acquire, lid, id, r);
      pthread_mutex_lock(&mutex);
      //服务器要求RETRY
      if (ret == lock_protocol::RETRY){
        while(!lock_cache_status_table[lid].retry)
          pthread_cond_wait(&lock_cache_status_table[lid].condForRetry, &mutex);
        lock_cache_status_table[lid].retry = false;
      }
      //直接获得锁
      else if (ret == lock_protocol::OK)
      {
        lock_cache_status_table[lid].status = LOCKED;
        break;
      }
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	of<<"RELEASE"<<lid<<";";
  pthread_mutex_lock(&mutex);
  int ret = lock_protocol::OK;
  if (!lock_cache_status_table.count(lid)){
  //do nothing
    pthread_mutex_unlock(&mutex);
    return ret;
  }

  if (lock_cache_status_table[lid].revoke){
      lock_cache_status_table[lid].revoke = false;
      int r;
      pthread_mutex_unlock(&mutex);
    lu->dorelease(lid);
	of<<"FLUSH"<<lid<<";";
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);
      lock_cache_status_table[lid].status = NONE;
      pthread_cond_signal(&lock_cache_status_table[lid].cond);
    }
  else{
    switch (lock_cache_status_table[lid].status)
    {
    case LOCKED:
      lock_cache_status_table[lid].status = FREE;
      pthread_cond_signal(&lock_cache_status_table[lid].cond);
      break;
    case FREE:
  //do nothing
      pthread_mutex_unlock(&mutex);
      return ret;
    case RELEASING:
      int r;
      pthread_mutex_unlock(&mutex);
    lu->dorelease(lid);
	of<<"FLUSH"<<lid<<";";
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);
      lock_cache_status_table[lid].status = NONE;
      pthread_cond_signal(&lock_cache_status_table[lid].cond);
      break;
    default:
      break;
    }
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  pthread_mutex_lock(&mutex);
  int ret = rlock_protocol::OK;
  if (!lock_cache_status_table.count(lid)){
  }
  switch(lock_cache_status_table[lid].status){
    case NONE:
    case RELEASING:
    case ACQUIRING:
      lock_cache_status_table[lid].revoke = true;
      break;
    case FREE:
      lock_cache_status_table[lid].status = RELEASING;
      int r;
      pthread_mutex_unlock(&mutex);     
    lu->dorelease(lid);
	of<<"FLUSH"<<lid<<";";
      ret = cl->call(lock_protocol::release, lid, id, r);
      pthread_mutex_lock(&mutex);  
      lock_cache_status_table[lid].status = NONE;
      pthread_cond_signal(&lock_cache_status_table[lid].cond);
      break;
    case LOCKED:
      lock_cache_status_table[lid].status = RELEASING;
      break;
    default:
      break;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  pthread_mutex_lock(&mutex);
  int ret = rlock_protocol::OK;
  pthread_cond_signal(&lock_cache_status_table[lid].condForRetry);
  lock_cache_status_table[lid].retry = true;
  pthread_mutex_unlock(&mutex);
  return ret;
}



