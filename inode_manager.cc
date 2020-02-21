#include "inode_manager.h"
#include  <string>
#include <ctime>

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

//检查block的合法性
bool is_blockrw_legal(blockid_t id, const char *buf)
{
  if(id<0 || id>=BLOCK_NUM ||!buf)
    return false;
  return true;
}

void
disk::read_block(blockid_t id, char *buf)
{
  if(is_blockrw_legal(id,buf))
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if(is_blockrw_legal(id,buf))
  memcpy(blocks[id], buf, BLOCK_SIZE);
}


// block layer -----------------------------------------

int 
block_manager::find_fit_block(int begin, int end, blockid_t start){
    for (blockid_t hover = begin; hover < end; hover++) {
        int is_used = using_blocks[hover];
        if (is_used == 0) {
            using_blocks[hover] = 1;
            next_block = (hover + 1) == BLOCK_NUM ? start : hover + 1;
            return hover;
        }
    }
    return -1;
}

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
    blockid_t start = IBLOCK(INODE_NUM, sb.nblocks) + 1;
    blockid_t res;
    //next fit
    if((res=find_fit_block(next_block,BLOCK_NUM,start))!=-1||
    //first fit
    (res=find_fit_block(start,next_block,start))!=-1)
        return res;
    exit(0);
}

void
block_manager::free_block(uint32_t id)
{
  using_blocks[id] = 0;
}

// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  // format the disk
  next_block = IBLOCK(INODE_NUM, sb.nblocks) + 1;
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
    bm = new block_manager();
    struct inode root;
    root.type = extent_protocol::T_DIR;
    root.size = 0;
    root.atime = (unsigned) std::time(0);
    root.mtime = (unsigned) std::time(0);
    root.ctime = (unsigned) std::time(0);
    put_inode(1, &root);
    next_inum = 2;
}


uint32_t
inode_manager::alloc_inode(uint32_t type) {
    struct inode *ino = NULL;
    uint32_t hover;
    for (hover = next_inum; hover < INODE_NUM+1; hover++) {
        ino = get_inode(hover);
        if (ino->type == 0) {
          goto found;
        } else free(ino);
    }
    for (hover = 2; hover < next_inum; hover++) {
            ino = get_inode(hover);
            if (ino->type == 0) {
            goto found;
            } else free(ino);
    }
    exit(0);
found:
    next_inum = (hover + 1) > INODE_NUM ? 2 : (hover + 1);
    ino->type = (short) type;
    ino->size = 0;
    ino->atime = (unsigned) std::time(0);
    ino->mtime = (unsigned) std::time(0);
    ino->ctime = (unsigned) std::time(0);
    put_inode(hover, ino);
    free(ino);
    return hover;
}

void
inode_manager::free_inode(uint32_t inum) {

    struct inode *ino = get_inode(inum);
    if (!ino || !(ino->type)) {
        exit(0);
    }
    ino->type = 0;
    put_inode(inum, ino);
    free(ino);
    return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint32_t inum) {
    struct inode *ino, *ino_disk;
    char buf[BLOCK_SIZE];
    if (inum <= 0 || inum > INODE_NUM) {
	return nullptr;
    }
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;
    ino = (struct inode *) malloc(sizeof(struct inode));
    *ino = *ino_disk;

    return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;
    *ino_disk = *ino;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}


/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf, int *size) {
    struct inode *ino = get_inode(inum); 
    std::string content;
    *size = ino->size;
    if (ino->size == 0)
        return;
    if (ino->size > MAXFILE * BLOCK_SIZE)
        exit(0);
    uint32_t block_num = ((ino->size - 1) / BLOCK_SIZE + 1);
    char *rv = (char *) malloc(BLOCK_NUM * BLOCK_SIZE);
    for (uint32_t nth = 0; nth < block_num; nth++) {
        read_nth_block(ino, nth, content);
        memcpy(rv + nth * BLOCK_SIZE, content.data(), BLOCK_SIZE);
    }
    *buf = rv;
    ino->atime = (unsigned) std::time(0);
    put_inode(inum, ino);
    free(ino);
}


/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size) {


    struct inode *ino = get_inode(inum);
    std::string content;
    uint32_t size_before = ino->size;
    uint32_t size_after = (uint32_t) size;

    uint32_t blnum_before = size_before == 0 ? 0 : ((size_before - 1) / BLOCK_SIZE + 1);
    uint32_t blnum_after = size_after == 0 ? 0 : ((size_after - 1) / BLOCK_SIZE + 1);

    if (blnum_after < blnum_before) {
        for (uint32_t start = blnum_after; start < blnum_before; start++)
            free_nth_block(ino, start);
    } else if (blnum_after > blnum_before) {
        for (uint32_t start = blnum_before; start < blnum_after; start++)
            alloc_nth_block(ino, start, content, false);
    }
    ino->size = (unsigned int) size;

    if (blnum_after != 0) {
        uint32_t start = 0;
        for (; start + 1 < blnum_after; start++) {
            content.assign(buf + BLOCK_SIZE * start, BLOCK_SIZE);
            write_nth_block(ino, start, content);
        }

        uint32_t padding_bytes = blnum_after * BLOCK_SIZE - size;
        uint32_t tail_bytes = BLOCK_SIZE - padding_bytes;

        content.assign(buf + BLOCK_SIZE * start, tail_bytes);

        content.resize(BLOCK_SIZE);

        write_nth_block(ino, blnum_after - 1, content);
    }
    ino->atime = (unsigned) std::time(0);
    ino->mtime = (unsigned) std::time(0);
    ino->ctime = (unsigned) std::time(0);
    put_inode(inum, ino);
    free(ino);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a) {

    struct inode *ino = get_inode(inum);
    if (!ino) {
        return;
    }
    a.type = (uint32_t) ino->type;
    a.atime = ino->atime;
    a.ctime = ino->ctime;
    a.mtime = ino->mtime;
    a.size = ino->size;
    free(ino);

}

void
inode_manager::remove_file(uint32_t inum) {

    struct inode *ino = get_inode(inum);
    uint32_t size = ino->size;
    uint32_t block_num = size == 0 ? 0 : ((size - 1) / BLOCK_SIZE + 1);

    for (uint32_t start = 0; start < block_num; start++) {
        free_nth_block(ino, start);
    }
    free_inode(inum);
    free(ino);

}


void inode_manager::read_nth_block(struct inode *ino, uint32_t nth, std::string &buf) {
    blockid_t bl_id = get_nth_block(ino, nth);
    char content[BLOCK_SIZE];
    bm->read_block(bl_id, content);
    buf.assign(content, BLOCK_SIZE);

}

void inode_manager::write_nth_block(struct inode *ino, uint32_t nth, std::string &buf) {
    blockid_t bl_id = get_nth_block(ino, nth);
    bm->write_block(bl_id, buf.data());
}


void inode_manager::alloc_nth_block(struct inode *ino, uint32_t nth, std::string &buf, bool to_write) {
    blockid_t bl_id = bm->alloc_block();
    if (to_write)
        bm->write_block(bl_id, buf.data());
    if (nth < NDIRECT)
        ino->blocks[nth] = bl_id;
    else {
        blockid_t inblock_id = ino->blocks[NDIRECT];
        char inblock[BLOCK_SIZE];
        bm->read_block(inblock_id, inblock);
        (((blockid_t *) inblock)[nth - NDIRECT]) = bl_id;
        bm->write_block(inblock_id, inblock);
    }
}


blockid_t inode_manager::get_nth_block(struct inode *ino, uint32_t nth) {
    if (nth < NDIRECT)
        return ino->blocks[nth];
    blockid_t inblock_id = ino->blocks[NDIRECT];
    char inblock[BLOCK_SIZE];
    bm->read_block(inblock_id, inblock);
    return (((blockid_t *) inblock)[nth - NDIRECT]);


}

void inode_manager::free_nth_block(struct inode *ino, uint32_t nth) {
    blockid_t id = get_nth_block(ino, nth);
    bm->free_block(id);
}



