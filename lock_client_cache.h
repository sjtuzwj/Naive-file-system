// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include <map>
#include<iostream>
#include<fstream>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include "extent_client_cache.h"


// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};


class lock_user : public lock_release_user {
private:
  extent_client_cache *ec;
public:
  lock_user(extent_client_cache *e) : ec(e) {}
  // dorelease to keep consistency(however write and symlink some bugs)
  void dorelease(lock_protocol::lockid_t lid){
    ec->flush(lid); 
  }
};
class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  //STATUS RECOMMENDED
  enum lock_client_cache_status 
  {NONE, FREE, LOCKED, ACQUIRING, RELEASING};
  class lock_cache_status
  {
  public:
    lock_cache_status():
    revoke(false),retry(false),
    status(NONE),cond(PTHREAD_COND_INITIALIZER),condForRetry(PTHREAD_COND_INITIALIZER)
    {}
    lock_client_cache_status status;
    pthread_cond_t cond;
    pthread_cond_t condForRetry;
    bool revoke;
    bool retry;
  };
  //lock table
  std::map<lock_protocol::lockid_t, lock_cache_status> lock_cache_status_table;
  static pthread_mutex_t mutex;
 public:
  static int last_port;
std::ofstream of;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
