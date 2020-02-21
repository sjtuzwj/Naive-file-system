#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include "extent_client_cache.h"
#include <vector>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct symlinkinfo
  {
    /* data */
    unsigned long long size;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };

  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  std::string encode(const std::string &);
  std::string uncode(const std::string &);

  int addfile(inum, const char *, mode_t, inum &,extent_protocol::types);
 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);
  bool issymlink(inum);


  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int getsymlink(inum, symlinkinfo &);

  int setattr(inum, size_t);
  int find(inum, const char *, bool &, inum &);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);
  int symlink(inum,const char *, const char *, inum &);
  int readlink(inum, std::string &);
  /** you may need to add symbolic link related methods here.*/
};


class LockGuard {
public:
  LockGuard(lock_client *lc, lock_protocol::lockid_t lid) : m_lc(lc), m_lid(lid)
  {
    m_lc->acquire(m_lid);
  }

  ~LockGuard()
  {
    m_lc->release(m_lid);
  }
private:
  lock_client *m_lc;
  lock_protocol::lockid_t m_lid;
};

#endif 
