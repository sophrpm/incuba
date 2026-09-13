#include "managers/FileSystemManager.hpp"

#include <ctime>
#include <fstream>

#include "utils/BinaryUtils.hpp"
#include "managers/DiskManager.hpp"

using namespace std;


//formatea una particion como ext2
bool FileSystemManager::formatExt2(const string& path, int partitionStart, int partitionSize, string& message) const {
    if (partitionStart < 0 || partitionSize <= static_cast<int>(sizeof(SuperBlock)) || static_cast<long long>(partitionStart) + partitionSize > BinaryUtils::getFileSize(path)){
        message = "Error: la particion no tiene un tamaño valido.";
        return false;
    }

    int inodeCount = calculateInodes(partitionSize);

    if (inodeCount < 2){
        message = "Error: la particion es demasiado pequeña para crear EXT2.";
        return false;
    }

    //limpia la particion antes del formato
    if (!clearPartition(path, partitionStart, partitionSize)){
        message = "Error: no se pudo limpiar la particion.";
        return false;
    }

    SuperBlock superBlock = createSuperBlock(partitionStart, partitionSize, inodeCount);

    //escribe el superbloque inicial
    if (!writeSuperBlock(path, partitionStart, superBlock)){
        message = "Error: no se pudo escribir el superbloque.";
        return false;
    }

    //inicia bitmap de inodos
    if (!initializeInodeBitmap(path, superBlock)){
        message = "Error: no se pudo iniciar el bitmap de inodos.";
        return false;
    }

    //inicia bitmap de bloques
    if (!initializeBlockBitmap(path, superBlock)){
        message = "Error: no se pudo iniciar el bitmap de bloques.";
        return false;
    }

    //inicia tabla de inodos
    if (!initializeInodes(path, superBlock)){
        message = "Error: no se pudo iniciar la tabla de inodos.";
        return false;
    }

    //inicia area de bloques
    if (!initializeBlocks(path, superBlock)){
        message = "Error: no se pudo iniciar el area de bloques.";
        return false;
    }

    //crea raiz y users.txt
    if (!createRoot(path, partitionStart, superBlock)){
        message = "Error: no se pudo crear la raiz y users.txt.";
        return false;
    }

    //guarda cambios finales
    if (!writeSuperBlock(path, partitionStart, superBlock)){
        message = "Error: no se pudo actualizar el superbloque.";
        return false;
    }

    message = "Sistema de archivos EXT2 creado correctamente.";
    return true;
}


//lee el superbloque de una particion
bool FileSystemManager::readSuperBlock(const string& path, int partitionStart, SuperBlock& superBlock) const {
    if (!BinaryUtils::readStruct(path, partitionStart, superBlock)){
        return false;
    }
    if (superBlock.s_magic != 0xEF53){
        return true;
    }
    DiskManager diskManager;
    MBR mbr;
    if (!diskManager.readMBR(path, mbr)){
        return false;
    }
    int partitionSize = 0;
    for (const Partition& partition : mbr.mbr_partitions){
        if (partition.part_type == 'P' && partition.part_start == partitionStart){
            partitionSize = partition.part_s;
        }
    }
    int count = calculateInodes(partitionSize);
    long long inodeStart = static_cast<long long>(partitionStart) + sizeof(SuperBlock) + 4LL * count;
    long long blockStart = inodeStart + static_cast<long long>(count) * sizeof(Inode);
    bool validCounts = count >= 2 && superBlock.s_filesystem_type == 2 && superBlock.s_inodes_count == count && superBlock.s_blocks_count == 3 * count;
    bool validSizes = superBlock.s_inode_s == static_cast<int>(sizeof(Inode)) && superBlock.s_block_s == 64;
    bool validBitmaps = superBlock.s_bm_inode_start == partitionStart + static_cast<int>(sizeof(SuperBlock)) && superBlock.s_bm_block_start == superBlock.s_bm_inode_start + count;
    bool validTables = superBlock.s_inode_start == inodeStart && superBlock.s_block_start == blockStart;
    return validCounts && validSizes && validBitmaps && validTables;

}


//escribe cambios del superbloque
bool FileSystemManager::writeSuperBlock(const string& path, int partitionStart, const SuperBlock& superBlock) const {
    return BinaryUtils::writeStruct(path, partitionStart, superBlock);
}


//lee un inodo por su posicion
bool FileSystemManager::readInode(const string& path, const SuperBlock& superBlock, int inodeIndex, Inode& inode) const {
    if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
        return false;
    }

    int inodePosition = superBlock.s_inode_start + inodeIndex * superBlock.s_inode_s;

    return BinaryUtils::readStruct(path, inodePosition, inode);
}


//escribe un inodo por su posicion
bool FileSystemManager::writeInode(const string& path, const SuperBlock& superBlock, int inodeIndex, const Inode& inode) const {
    if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
        return false;
    }

    int inodePosition = superBlock.s_inode_start + inodeIndex * superBlock.s_inode_s;

    return BinaryUtils::writeStruct(path, inodePosition, inode);
}


//lee un bloque carpeta
bool FileSystemManager::readFolderBlock(const string& path, const SuperBlock& superBlock, int blockIndex, FolderBlock& folderBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::readStruct(path, blockPosition, folderBlock);
}


//escribe un bloque carpeta
bool FileSystemManager::writeFolderBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const FolderBlock& folderBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::writeStruct(path, blockPosition, folderBlock);
}


//lee un bloque archivo
bool FileSystemManager::readFileBlock(const string& path, const SuperBlock& superBlock, int blockIndex, FileBlock& fileBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::readStruct(path, blockPosition, fileBlock);
}


//escribe un bloque archivo
bool FileSystemManager::writeFileBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const FileBlock& fileBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::writeStruct(path, blockPosition, fileBlock);
}


//lee un bloque de apuntadores
bool FileSystemManager::readPointerBlock(const string& path, const SuperBlock& superBlock, int blockIndex, PointerBlock& pointerBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::readStruct(path, blockPosition, pointerBlock);
}


//escribe un bloque de apuntadores
bool FileSystemManager::writePointerBlock(const string& path, const SuperBlock& superBlock, int blockIndex, const PointerBlock& pointerBlock) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    return BinaryUtils::writeStruct(path, blockPosition, pointerBlock);
}


//busca un inodo libre en bitmap
int FileSystemManager::findFreeInode(const string& path, const SuperBlock& superBlock) const {
    ifstream file(path, ios::binary);
    file.seekg(superBlock.s_bm_inode_start);
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        char value;
        if (!file.get(value)){
            return -1;
        }
        if (value == '0'){
            return inodeIndex;
        }
    }

    return -1;
}


//busca un bloque libre en bitmap
int FileSystemManager::findFreeBlock(const string& path, const SuperBlock& superBlock) const {
    ifstream file(path, ios::binary);
    file.seekg(superBlock.s_bm_block_start);
    for (int blockIndex = 0; blockIndex < superBlock.s_blocks_count; blockIndex++){
        char value;
        if (!file.get(value)){
            return -1;
        }
        if (value == '0'){
            return blockIndex;
        }
    }

    return -1;
}


//reserva un inodo libre
int FileSystemManager::allocateInode(const string& path, int partitionStart, SuperBlock& superBlock) const {
    int inodeIndex = findFreeInode(path, superBlock);

    if (inodeIndex == -1){
        return -1;
    }

    if (!writeInodeBitmap(path, superBlock, inodeIndex, '1')){
        return -1;
    }

    superBlock.s_free_inodes_count--;

    updateFirstFree(path, superBlock);

    if (!writeSuperBlock(path, partitionStart, superBlock)){
        writeInodeBitmap(path, superBlock, inodeIndex, '0');

        superBlock.s_free_inodes_count++;

        updateFirstFree(path, superBlock);

        return -1;
    }

    return inodeIndex;
}


//reserva un bloque libre
int FileSystemManager::allocateBlock(const string& path, int partitionStart, SuperBlock& superBlock) const {
    int blockIndex = findFreeBlock(path, superBlock);

    if (blockIndex == -1){
        return -1;
    }

    if (!writeBlockBitmap(path, superBlock, blockIndex, '1')){
        return -1;
    }

    superBlock.s_free_blocks_count--;

    updateFirstFree(path, superBlock);

    if (!writeSuperBlock(path, partitionStart, superBlock)){
        writeBlockBitmap(path, superBlock, blockIndex, '0');

        superBlock.s_free_blocks_count++;

        updateFirstFree(path, superBlock);

        return -1;
    }

    return blockIndex;
}


//libera un inodo
bool FileSystemManager::freeInode(const string& path, int partitionStart, SuperBlock& superBlock, int inodeIndex) const {
    if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
        return false;
    }

    char value;

    if (!readInodeBitmap(path, superBlock, inodeIndex, value)){
        return false;
    }

    //ya estaba libre
    if (value == '0'){
        return true;
    }

    if (!writeInodeBitmap(path, superBlock, inodeIndex, '0')){
        return false;
    }

    //limpia tambien la estructura del inodo
    Inode emptyInode;

    if (!writeInode(path, superBlock, inodeIndex, emptyInode)){
        writeInodeBitmap(path, superBlock, inodeIndex, '1');
        return false;
    }

    superBlock.s_free_inodes_count++;

    updateFirstFree(path, superBlock);

    return writeSuperBlock(path, partitionStart, superBlock);
}


//libera un bloque
bool FileSystemManager::freeBlock(const string& path, int partitionStart, SuperBlock& superBlock, int blockIndex) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    char value;

    if (!readBlockBitmap(path, superBlock, blockIndex, value)){
        return false;
    }

    //ya estaba libre
    if (value == '0'){
        return true;
    }

    if (!writeBlockBitmap(path, superBlock, blockIndex, '0')){
        return false;
    }

    int blockPosition = superBlock.s_block_start + blockIndex * superBlock.s_block_s;

    //limpia los 64 bytes del bloque
    if (!BinaryUtils::clearSpace(path, blockPosition, superBlock.s_block_s)){
        writeBlockBitmap(path, superBlock, blockIndex, '1');
        return false;
    }

    superBlock.s_free_blocks_count++;

    updateFirstFree(path, superBlock);

    return writeSuperBlock(path, partitionStart, superBlock);
}


//lee valor del bitmap de inodos
bool FileSystemManager::readInodeBitmap(const string& path, const SuperBlock& superBlock, int inodeIndex, char& value) const {
    if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
        return false;
    }

    fstream file(path, ios::in | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekg(superBlock.s_bm_inode_start + inodeIndex, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    file.read(&value, sizeof(char));

    bool readOk = file.good();

    file.close();

    return readOk;
}


//lee valor del bitmap de bloques
bool FileSystemManager::readBlockBitmap(const string& path, const SuperBlock& superBlock, int blockIndex, char& value) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    fstream file(path, ios::in | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekg(superBlock.s_bm_block_start + blockIndex, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    file.read(&value, sizeof(char));

    bool readOk = file.good();

    file.close();

    return readOk;
}


//calcula cantidad de inodos para ext2
int FileSystemManager::calculateInodes(int partitionSize) const {
    int availableSpace = partitionSize - static_cast<int>(sizeof(SuperBlock));

    //n + 3n + n*sizeof(inode) + 3n*sizeof(block)
    int structureSize = 4 + static_cast<int>(sizeof(Inode)) + 3 * 64;

    if (availableSpace <= 0 || structureSize <= 0){
        return 0;
    }

    return availableSpace / structureSize;
}


//inicia el superbloque
SuperBlock FileSystemManager::createSuperBlock(int partitionStart, int, int inodeCount) const {
    SuperBlock superBlock;

    superBlock.s_filesystem_type = 2;

    superBlock.s_inodes_count = inodeCount;
    superBlock.s_blocks_count = 3 * inodeCount;

    superBlock.s_free_blocks_count = superBlock.s_blocks_count;
    superBlock.s_free_inodes_count = superBlock.s_inodes_count;

    //la particion ya estaba montada antes de hacer mkfs
    superBlock.s_mtime = time(nullptr);
    superBlock.s_umtime = 0;
    superBlock.s_mnt_count = 1;

    superBlock.s_magic = 0xEF53;

    superBlock.s_inode_s = static_cast<int>(sizeof(Inode));
    superBlock.s_block_s = 64;

    //inicio de cada area dentro de la particion
    superBlock.s_bm_inode_start = partitionStart + static_cast<int>(sizeof(SuperBlock));

    superBlock.s_bm_block_start =
        superBlock.s_bm_inode_start + inodeCount;

    superBlock.s_inode_start =
        superBlock.s_bm_block_start + 3 * inodeCount;

    superBlock.s_block_start =
        superBlock.s_inode_start + inodeCount * superBlock.s_inode_s;

    //al inicio todos estan libres
    superBlock.s_firts_ino = superBlock.s_inode_start;
    superBlock.s_first_blo = superBlock.s_block_start;

    return superBlock;
}


//limpia la particion antes del formato
bool FileSystemManager::clearPartition(const string& path, int partitionStart, int partitionSize) const {
    return BinaryUtils::clearSpace(path, partitionStart, partitionSize);
}


//inicia bitmap de inodos
bool FileSystemManager::initializeInodeBitmap(const string& path, const SuperBlock& superBlock) const {
    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(superBlock.s_bm_inode_start, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    //inicia todos los inodos libres
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        char value = '0';

        file.write(&value, sizeof(char));

        if (!file.good()){
            file.close();
            return false;
        }
    }

    file.close();

    return true;
}


//inicia bitmap de bloques
bool FileSystemManager::initializeBlockBitmap(const string& path, const SuperBlock& superBlock) const {
    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(superBlock.s_bm_block_start, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    //inicia todos los bloques libres
    for (int blockIndex = 0; blockIndex < superBlock.s_blocks_count; blockIndex++){
        char value = '0';

        file.write(&value, sizeof(char));

        if (!file.good()){
            file.close();
            return false;
        }
    }

    file.close();

    return true;
}


//inicia tabla de inodos
bool FileSystemManager::initializeInodes(const string& path, const SuperBlock& superBlock) const {
    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(superBlock.s_inode_start, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    //escribe todos los inodos vacios
    for (int inodeIndex = 0; inodeIndex < superBlock.s_inodes_count; inodeIndex++){
        Inode emptyInode;

        file.write(reinterpret_cast<const char*>(&emptyInode), sizeof(Inode));

        if (!file.good()){
            file.close();
            return false;
        }
    }

    file.close();

    return true;
}


//inicia area de bloques
bool FileSystemManager::initializeBlocks(const string& path, const SuperBlock& superBlock) const {
    int blocksSize = superBlock.s_blocks_count * superBlock.s_block_s;

    return BinaryUtils::clearSpace(path, superBlock.s_block_start, blocksSize);
}


//crea raiz y users.txt
bool FileSystemManager::createRoot(const string& path, int partitionStart, SuperBlock& superBlock) const {
    if (superBlock.s_inodes_count < 2 || superBlock.s_blocks_count < 2){
        return false;
    }

    //reserva inodo 0 para /
    if (!writeInodeBitmap(path, superBlock, 0, '1')){
        return false;
    }

    //reserva inodo 1 para users.txt
    if (!writeInodeBitmap(path, superBlock, 1, '1')){
        return false;
    }

    //reserva bloque 0 para /
    if (!writeBlockBitmap(path, superBlock, 0, '1')){
        return false;
    }

    //reserva bloque 1 para users.txt
    if (!writeBlockBitmap(path, superBlock, 1, '1')){
        return false;
    }

    time_t currentTime = time(nullptr);


    //crea inodo raiz
    Inode rootInode;

    rootInode.i_uid = 1;
    rootInode.i_gid = 1;
    rootInode.i_s = static_cast<int>(sizeof(FolderBlock));

    rootInode.i_atime = currentTime;
    rootInode.i_ctime = currentTime;
    rootInode.i_mtime = currentTime;

    rootInode.i_block[0] = 0;

    //0 representa carpeta
    rootInode.i_type = '0';

    //la raiz siempre trabaja con 777
    BinaryUtils::copyToFixedChar(rootInode.i_perm, 3, "777");


    //crea users.txt
    string usersContent = "1,G,root\n1,U,root,root,123\n";

    Inode usersInode;

    usersInode.i_uid = 1;
    usersInode.i_gid = 1;
    usersInode.i_s = static_cast<int>(usersContent.size());

    usersInode.i_atime = currentTime;
    usersInode.i_ctime = currentTime;
    usersInode.i_mtime = currentTime;

    usersInode.i_block[0] = 1;

    //1 representa archivo
    usersInode.i_type = '1';

    BinaryUtils::copyToFixedChar(usersInode.i_perm, 3, "664");


    //crea bloque carpeta de /
    FolderBlock rootBlock;

    BinaryUtils::copyToFixedChar(rootBlock.b_content[0].b_name, 12, ".");
    rootBlock.b_content[0].b_inodo = 0;

    BinaryUtils::copyToFixedChar(rootBlock.b_content[1].b_name, 12, "..");
    rootBlock.b_content[1].b_inodo = 0;

    BinaryUtils::copyToFixedChar(rootBlock.b_content[2].b_name, 12, "users.txt");
    rootBlock.b_content[2].b_inodo = 1;


    //crea primer bloque de users.txt
    FileBlock usersBlock;

    BinaryUtils::copyToFixedChar(usersBlock.b_content, 64, usersContent);


    //escribe inodos
    if (!writeInode(path, superBlock, 0, rootInode)){
        return false;
    }

    if (!writeInode(path, superBlock, 1, usersInode)){
        return false;
    }


    //escribe bloques
    if (!writeFolderBlock(path, superBlock, 0, rootBlock)){
        return false;
    }

    if (!writeFileBlock(path, superBlock, 1, usersBlock)){
        return false;
    }


    //ya usamos dos inodos y dos bloques
    superBlock.s_free_inodes_count -= 2;
    superBlock.s_free_blocks_count -= 2;

    updateFirstFree(path, superBlock);

    return writeSuperBlock(path, partitionStart, superBlock);
}


//cambia valor del bitmap de inodos
bool FileSystemManager::writeInodeBitmap(const string& path, const SuperBlock& superBlock, int inodeIndex, char value) const {
    if (inodeIndex < 0 || inodeIndex >= superBlock.s_inodes_count){
        return false;
    }

    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(superBlock.s_bm_inode_start + inodeIndex, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    file.write(&value, sizeof(char));

    bool writeOk = file.good();

    file.close();

    return writeOk;
}


//cambia valor del bitmap de bloques
bool FileSystemManager::writeBlockBitmap(const string& path, const SuperBlock& superBlock, int blockIndex, char value) const {
    if (blockIndex < 0 || blockIndex >= superBlock.s_blocks_count){
        return false;
    }

    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(superBlock.s_bm_block_start + blockIndex, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    file.write(&value, sizeof(char));

    bool writeOk = file.good();

    file.close();

    return writeOk;
}


//actualiza direccion del primer inodo y bloque libre
void FileSystemManager::updateFirstFree(const string& path, SuperBlock& superBlock) const {
    int freeInode = findFreeInode(path, superBlock);
    int freeBlock = findFreeBlock(path, superBlock);

    //guarda la direccion fisica del inodo
    if (freeInode == -1){
        superBlock.s_firts_ino = -1;
    } else {
        superBlock.s_firts_ino =
            superBlock.s_inode_start + freeInode * superBlock.s_inode_s;
    }

    //guarda la direccion fisica del bloque
    if (freeBlock == -1){
        superBlock.s_first_blo = -1;
    } else {
        superBlock.s_first_blo =
            superBlock.s_block_start + freeBlock * superBlock.s_block_s;
    }
}
