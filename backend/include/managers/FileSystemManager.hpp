#ifndef FILE_SYSTEM_MANAGER_HPP
#define FILE_SYSTEM_MANAGER_HPP

#include <string>

#include "structures/Ext2Structures.hpp"

using namespace std;


//maneja la estructura principal de ext2
class FileSystemManager {
public:

    //formatea una particion como ext2
    bool formatExt2(const string& path, int partitionStart, int partitionSize, string& message) const;

    //lee el superbloque de una particion
    bool readSuperBlock(const string& path, int partitionStart, SuperBlock& superBlock) const;

    //escribe cambios del superbloque
    bool writeSuperBlock(const string& path, int partitionStart, const SuperBlock& superBlock) const;

    //lee un inodo por su posicion
    bool readInode(const string& path, const SuperBlock& superBlock, int inodeIndex, Inode& inode) const;

    //escribe un inodo por su posicion
    bool writeInode(const string& path, const SuperBlock& superBlock, int inodeIndex, const Inode& inode) const;

    //lee un bloque carpeta
    bool readFolderBlock(const string& path, const SuperBlock& superBlock, int blockIndex, FolderBlock& folderBlock) const;

    //escribe un bloque carpeta
    bool writeFolderBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const FolderBlock& folderBlock) const;

    //lee un bloque archivo
    bool readFileBlock(const string& path, const SuperBlock& superBlock, int blockIndex, FileBlock& fileBlock) const;

    //escribe un bloque archivo
    bool writeFileBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const FileBlock& fileBlock) const;

    //lee un bloque de apuntadores
    bool readPointerBlock(const string& path, const SuperBlock& superBlock, int blockIndex, PointerBlock& pointerBlock) const;

    //escribe un bloque de apuntadores
    bool writePointerBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const PointerBlock& pointerBlock) const;

    //busca un inodo libre en bitmap
    int findFreeInode(const string& path, const SuperBlock& superBlock) const;

    //busca un bloque libre en bitmap
    int findFreeBlock(const string& path, const SuperBlock& superBlock) const;

    //reserva un inodo libre
    int allocateInode(const string& path, int partitionStart, SuperBlock& superBlock) const;

    //reserva un bloque libre
    int allocateBlock(const string& path, int partitionStart, SuperBlock& superBlock) const;

    //libera un inodo
    bool freeInode(const string& path, int partitionStart, SuperBlock& superBlock, int inodeIndex) const;

    //libera un bloque
    bool freeBlock(const string& path, int partitionStart, SuperBlock& superBlock, int blockIndex) const;

    //lee valor del bitmap de inodos
    bool readInodeBitmap(const string& path, const SuperBlock& superBlock, int inodeIndex, char& value) const;

    //lee valor del bitmap de bloques
    bool readBlockBitmap(const string& path, const SuperBlock& superBlock, int blockIndex, char& value) const;

private:

    //calcula cantidad de inodos para ext2
    int calculateInodes(int partitionSize) const;

    //inicia el superbloque
    SuperBlock createSuperBlock(int partitionStart, int partitionSize, int inodeCount) const;

    //limpia la particion antes del formato
    bool clearPartition(const string& path, int partitionStart, int partitionSize) const;

    //inicia bitmap de inodos
    bool initializeInodeBitmap(const string& path, const SuperBlock& superBlock) const;

    //inicia bitmap de bloques
    bool initializeBlockBitmap(const string& path, const SuperBlock& superBlock) const;

    //inicia tabla de inodos
    bool initializeInodes(const string& path, const SuperBlock& superBlock) const;

    //inicia area de bloques
    bool initializeBlocks(const string& path, const SuperBlock& superBlock) const;

    //crea raiz y users.txt
    bool createRoot(const string& path, int partitionStart, SuperBlock& superBlock) const;

    //cambia valor del bitmap de inodos
    bool writeInodeBitmap(const string& path, const SuperBlock& superBlock, int inodeIndex, char value) const;

    //cambia valor del bitmap de bloques
    bool writeBlockBitmap(const string& path, const SuperBlock& superBlock, int blockIndex, char value) const;

    //actualiza primer inodo y bloque libre
    void updateFirstFree(const string& path, SuperBlock& superBlock) const;
};


#endif
