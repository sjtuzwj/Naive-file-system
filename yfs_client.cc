// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client_cache(extent_dst);
  lc = new lock_client_cache(lock_dst,new lock_user((extent_client_cache*)(ec)));
   LockGuard lg(lc, 1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");

        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);

        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);

    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */
bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLK) {
        printf("isfile: %lld is a symlink\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a symlink\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a dir\n", inum);
    return false;
}

//根据文件名额外产生generation用于校验
std::string yfs_client::encode(const std::string &code)
{
    std::string buf = "";
    for (size_t i = 0; i < code.size(); i++)
    {
        //防止混入空格等字符
        buf += 'a'+code[i] / 16;
        buf += 'a'+code[i] % 16;
    }
    return buf;
}
std::string yfs_client::uncode(const std::string &code)
{
    std::string buf = "";
    for (size_t i = 0; i < code.size(); i += 2)
        buf += (code[i] - 'a')* 16 + code[i + 1] - 'a';
    return buf;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    LockGuard lg(lc, inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getsymlink(inum inum, symlinkinfo &sin1)
{
    int r = OK;
    printf("getlink %016llx\n", inum);
    extent_protocol::attr a;
    LockGuard lg(lc, inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    sin1.atime = a.atime;
    sin1.mtime = a.mtime;
    sin1.ctime = a.ctime;
    sin1.size = a.size;
    printf("getlink %016llx -> sz %llu\n", inum, sin1.size);
release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    LockGuard lg(lc, inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    std::string buf;
    extent_protocol::attr a;
    extent_protocol::status ret;
    LockGuard lg(lc, ino);
    if ((ret = ec->getattr(ino, a)) != extent_protocol::OK) {
      printf("error getting attr, return not OK\n");
      return ret;
    }
    ec->get(ino, buf);
    if (a.size < size) 
      buf += std::string(size - a.size, '\0');
    else if (a.size > size) 
      buf = buf.substr(0, size);
    ec->put(ino, buf);
    return r;
}

int
yfs_client::addfile(inum parent, const char *name, mode_t mode, inum &ino_out,extent_protocol::types t){
    int r = OK;
    bool found = false;
    inum inode;
    std::string buf;
    LockGuard lg(lc, parent);
    ec->create(t, ino_out);
    ec->get(parent, buf);
    buf += encode(std::string(name)) + ' ' + filename(ino_out) + ' ';
    ec->put(parent, buf);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return addfile(parent,name,mode,ino_out,extent_protocol::T_FILE);
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    return addfile(parent,name,mode,ino_out,extent_protocol::T_DIR);
}
int
yfs_client::find(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    std::string buf;
    extent_protocol::attr attr;
    ec->getattr(parent, attr);
    if (attr.type != extent_protocol::T_DIR)
    {
        return EXIST;
    }
    ec->get(parent, buf);
    std::stringstream ss(buf);
    std::string fileName, inode;
    for (size_t i = 0; i < buf.size(); i += fileName.size() + inode.size() + 2)
    {
        ss >> fileName >> inode;
        std::string name_buf = uncode(fileName);
        if (name_buf == std::string(name))
        {
            ino_out = n2i(inode);
            found = true;
            return r;
        }
    }
    found = false;
    return IOERR;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    LockGuard lg(lc, parent);
return find(parent,name,found,ino_out);
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    LockGuard lg(lc, dir);
    std::string buf;
    if (!isdir(dir))
    {
        return EXIST;
    }

    ec->get(dir, buf);
    std::stringstream ss(buf);
    std::string fileName,inode;

    for (size_t i = 0; i < buf.size(); i += fileName.size() + inode.size() + 2)
    {
        ss >> fileName >> inode;
        std::string name_buf = uncode(fileName);
        dirent buf_d;
        buf_d.name = name_buf;
        buf_d.inum = n2i(inode);
        list.push_back(buf_d);
    }
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    LockGuard lg(lc, ino);
    ec->get(ino, data);
    if (off <= data.size())
    {
        if (off + size <= data.size())
            data = data.substr(off, size);
        else
            data = data.substr(off, data.size() - off);
    }
    //超出范围了、
    else 
      data = "";
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    LockGuard lg(lc, ino);
    std::string buf;
    std::string dada_buf = std::string(data, size);
    ec->get(ino, buf);
    //文件增大
    if (size + off > buf.size())
        buf.resize(off + size, '\0');
    bytes_written = size;
    buf.replace(off, size, dada_buf);
    ec->put(ino, buf);
    ((extent_client_cache*)(ec))->flush(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    LockGuard lg(lc, parent);
    bool flag = false;
    std::string buf,result;
    ec->get(parent, buf);
    std::string fileName ,node;
    std::stringstream ss(buf);
    for (size_t i = 0; i < buf.size(); i +=fileName.size() + node.size() + 2)
    {
        ss >> fileName >> node;
        if (uncode(fileName)==std::string(name))
        {
            ec->remove(n2i(node));
            flag = true;
        }
        else
            result += fileName + ' ' + node + ' ';
    }
    if (!flag){
        return ENOENT;
    }
    ec->put(parent, result);
    return r;
}


int yfs_client::symlink(inum parent, const char *link, const char *name, inum &ino)
{  mode_t mode = 0;
    int r= addfile(parent,name,mode,ino,extent_protocol::T_SYMLK);
    LockGuard lg(lc, ino);
    ec->put(ino, std::string(link));

//woc ,lab4 symlink test crazy!!!!
    ((extent_client_cache*)(ec))->flush(ino);
    return r;
}

int yfs_client::readlink(inum ino, std::string &result)
{
    int r = OK;
    LockGuard lg(lc, ino); 
    ec->get(ino, result);
    return r;
}

