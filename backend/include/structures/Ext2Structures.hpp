#ifndef EXT2_STRUCTURES_HPP
#define EXT2_STRUCTURES_HPP

#include <ctime>

using namespace std;


//info main del sistema ext2
struct SuperBlock {
    int s_filesystem_type = 2;
    int s_inodes_count = 0;
    int s_blocks_count = 0;
    int s_free_blocks_count = 0;
    int s_free_inodes_count = 0;
    time_t s_mtime = 0;
    time_t s_umtime = 0;
    int s_mnt_count = 0;
    int s_magic = 0xEF53;
    int s_inode_s = 0;
    int s_block_s = 64;
    int s_firts_ino = 0;
    int s_first_blo = 0;
    int s_bm_inode_start = 0;
    int s_bm_block_start = 0;
    int s_inode_start = 0;
    int s_block_start = 0;
};


//info de un arch o carpeta
struct Inode {
    int i_uid = -1;
    int i_gid = -1;
    int i_s = 0;
    time_t i_atime = 0;
    time_t i_ctime = 0;
    time_t i_mtime = 0;
    int i_block[15];
    char i_type = '0';
    char i_perm[3] = {};

    //inicia todos los apuntadores vacios
    Inode(){
        for (int blockPos = 0; blockPos < 15; blockPos++){
            i_block[blockPos] = -1;
        }
    }
};


//entrada dentro de un bloque carpeta
struct Content {
    char b_name[12] = {};
    int b_inodo = -1;
};


//bloque para guardar archs o carpetas
struct FolderBlock {
    Content b_content[4];
};


//bloque para guardar contenido de arch
struct FileBlock {
    char b_content[64] = {};
};


//bloque para apuntadores indirectos
struct PointerBlock {
    int b_pointers[16];

    //inicia todos los apuntadores vacios
    PointerBlock(){
        for (int pointerPos = 0; pointerPos < 16; pointerPos++){
            b_pointers[pointerPos] = -1;
        }
    }
};


#endif
