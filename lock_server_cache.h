#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>


#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
  int nacquire;  
  enum lock_server_cache_status 
  {FREE, LOCKED, WAITING};

/*
The server's per-lock state should include whether it is held by some client, the ID (host name and port number) of that client, and the set of other clients waiting for that lock. The server needs to know the holding client's ID in order to sent it a revoke message when another client wants the lock. The server needs to know the set of waiting clients in order to send one of them a retry RPC when the holder releases the lock.
*/
  class lock_cache_status
  {
	public:
	std::string clientId;
	std::queue<std::string> clientQ;
  std::set<std::string> clientS;
	lock_server_cache_status status;
  };  
  std::map<lock_protocol::lockid_t,  lock_cache_status> lock_cache_status_table;
  static pthread_mutex_t mutex;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
