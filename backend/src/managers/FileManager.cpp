#include "managers/FileManager.hpp"

#include <ctime>
#include <algorithm>

#include "managers/FileSystemManager.hpp"
#include "managers/MountManager.hpp"
#include "state/AppState.hpp"
#include "utils/BinaryUtils.hpp"

using namespace std;


//cuenta datos y apuntadores para un archivo continuo
static int storageBlocks(int dataBlocks){
    int total = dataBlocks;
    if (dataBlocks > 12){
        total++;
    }
    if (dataBlocks > 28){
        int doubleBlocks = min(dataBlocks - 28, 256);
        total += 1 + (doubleBlocks + 15) / 16;
    }
    if (dataBlocks > 284){
        int tripleBlocks = dataBlocks - 284;
        total += 1 + (tripleBlocks + 255) / 256 + (tripleBlocks + 15) / 16;
    }
    return total;
}


//crea una carpeta dentro del sistema
bool FileManager::createDirectory(const string& path, bool recursive, AppState& appState, string& message) const {
    if (!appState.session.active){
        message = "Error: debe iniciar sesion para crear carpetas.";
        return false;
    }

    vector<string> pathParts = splitPath(path);

    if (pathParts.empty()){
        message = "Error: no se puede crear la carpeta raiz.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene un sistema EXT2 valido.";
        return false;
    }

    int currentInodeIndex = 0;
    Inode currentInode;

    if (!fileSystemManager.readInode(mountedPartition.path, superBlock, currentInodeIndex, currentInode)){
        message = "Error: no se pudo leer la carpeta raiz.";
        return false;
    }

    //recorre cada parte de la ruta
    for (size_t pathPos = 0; pathPos < pathParts.size(); pathPos++){
        string currentName = pathParts[pathPos];
        bool lastPart = pathPos == pathParts.size() - 1;

        if (currentName.size() > 12){
            message = "Error: el nombre " + currentName + " supera 12 caracteres.";
            return false;
        }

        if (currentInode.i_type != '0'){
            message = "Error: una parte de la ruta no corresponde a una carpeta.";
            return false;
        }

        int foundInodeIndex = findEntry(mountedPartition.path, superBlock, currentInode, currentName);

        //si ya existe continua por la ruta
        if (foundInodeIndex != -1){
            Inode foundInode;

            if (!fileSystemManager.readInode(mountedPartition.path, superBlock, foundInodeIndex, foundInode)){
                message = "Error: no se pudo leer una carpeta de la ruta.";
                return false;
            }

            if (foundInode.i_type != '0'){
                message = "Error: " + currentName + " ya existe y no es una carpeta.";
                return false;
            }

            if (lastPart){
                message = recursive ? "La carpeta ya existe; no se realizaron cambios." : "Error: la carpeta " + currentName + " ya existe.";
                return recursive;
            }

            currentInodeIndex = foundInodeIndex;
            currentInode = foundInode;
            continue;
        }

        //sin -p solo permite crear la ultima carpeta
        if (!lastPart && !recursive){
            message = "Error: no existe la carpeta padre " + currentName + ".";
            return false;
        }

        if (!canWrite(currentInode, appState.session.uid, appState.session.gid)){
            message = "Error: no tiene permiso de escritura en la carpeta padre.";
            return false;
        }

        int newInodeIndex = -1;

        if (!createSingleDirectory(mountedPartition.path, mountedPartition.start, superBlock, currentInodeIndex, currentName, appState.session.uid, appState.session.gid, newInodeIndex)){
            message = "Error: no se pudo crear la carpeta " + currentName + ".";
            return false;
        }

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, newInodeIndex, currentInode)){
            message = "Error: no se pudo leer la carpeta creada.";
            return false;
        }

        currentInodeIndex = newInodeIndex;
    }

    message = "Carpeta " + path + " creada correctamente.";
    return true;
}


//crea un archivo dentro del sistema
bool FileManager::createFile(const string& path, const string& content, bool recursive, bool overwrite, AppState& appState, string& message) const {
    if (!appState.session.active){
        message = "Error: debe iniciar sesion para crear archivos.";
        return false;
    }

    vector<string> pathParts = splitPath(path);

    if (pathParts.empty()){
        message = "Error: la ruta del archivo no es valida.";
        return false;
    }

    string fileName = pathParts.back();

    if (fileName.empty() || fileName.size() > 12){
        message = "Error: el nombre del archivo no puede superar 12 caracteres.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene un sistema EXT2 valido.";
        return false;
    }

    int parentInodeIndex = 0;
    Inode parentInode;

    if (!fileSystemManager.readInode(mountedPartition.path, superBlock, parentInodeIndex, parentInode)){
        message = "Error: no se pudo leer la carpeta raiz.";
        return false;
    }

    //recorre carpetas padre
    for (size_t pathPos = 0; pathPos + 1 < pathParts.size(); pathPos++){
        string directoryName = pathParts[pathPos];

        if (directoryName.size() > 12){
            message = "Error: el nombre " + directoryName + " supera 12 caracteres.";
            return false;
        }

        int foundInodeIndex = findEntry(mountedPartition.path, superBlock, parentInode, directoryName);

        if (foundInodeIndex == -1){
            if (!recursive){
                message = "Error: no existe la carpeta padre " + directoryName + ".";
                return false;
            }

            if (!canWrite(parentInode, appState.session.uid, appState.session.gid)){
                message = "Error: no tiene permiso para crear carpetas en la ruta.";
                return false;
            }

            int newDirectoryIndex = -1;

            if (!createSingleDirectory(mountedPartition.path, mountedPartition.start, superBlock, parentInodeIndex, directoryName, appState.session.uid, appState.session.gid, newDirectoryIndex)){
                message = "Error: no se pudo crear la carpeta " + directoryName + ".";
                return false;
            }

            foundInodeIndex = newDirectoryIndex;
        }

        Inode foundInode;

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, foundInodeIndex, foundInode)){
            message = "Error: no se pudo leer una carpeta de la ruta.";
            return false;
        }

        if (foundInode.i_type != '0'){
            message = "Error: " + directoryName + " no es una carpeta.";
            return false;
        }

        parentInodeIndex = foundInodeIndex;
        parentInode = foundInode;
    }

    if (!canWrite(parentInode, appState.session.uid, appState.session.gid)){
        message = "Error: no tiene permiso de escritura en la carpeta padre.";
        return false;
    }

    int existingInodeIndex = findEntry(mountedPartition.path, superBlock, parentInode, fileName);

    //si existe verifica si se reemplaza
    if (existingInodeIndex != -1){
        Inode existingInode;

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, existingInodeIndex, existingInode)){
            message = "Error: no se pudo leer el archivo existente.";
            return false;
        }

        if (existingInode.i_type != '1'){
            message = "Error: ya existe una carpeta con el nombre " + fileName + ".";
            return false;
        }

        if (!canWrite(existingInode, appState.session.uid, appState.session.gid)){
            message = "Error: no tiene permiso para modificar el archivo.";
            return false;
        }

        if (!overwrite){
            appState.pendingConfirmation.active = true;
            appState.pendingConfirmation.path = path;
            appState.pendingConfirmation.content = content;
            appState.pendingConfirmation.recursive = recursive;
            message = "El archivo ya existe.\n¿Desea sobrescribirlo? [y/n]:";
            return false;
        }

        if (!writeFileContent(mountedPartition.path, mountedPartition.start, superBlock, existingInodeIndex, existingInode, content)){
            message = "Error: no se pudo reemplazar el contenido del archivo.";
            return false;
        }

        message = "Archivo " + path + " actualizado correctamente.";
        return true;
    }

    if (!canWrite(parentInode, appState.session.uid, appState.session.gid)){
        message = "Error: no tiene permiso de escritura en la carpeta padre.";
        return false;
    }

    int newInodeIndex = -1;

    if (!createFileInode(mountedPartition.path, mountedPartition.start, superBlock, appState.session.uid, appState.session.gid, content, newInodeIndex)){
        message = "Error: no se pudo crear el inodo del archivo.";
        return false;
    }

    if (!addDirectoryEntry(mountedPartition.path, mountedPartition.start, superBlock, parentInodeIndex, parentInode, fileName, newInodeIndex)){
        Inode newInode;

        if (fileSystemManager.readInode(mountedPartition.path, superBlock, newInodeIndex, newInode)){
            freeFileBlocks(mountedPartition.path, mountedPartition.start, superBlock, newInode);
        }

        fileSystemManager.freeInode(mountedPartition.path, mountedPartition.start, superBlock, newInodeIndex);
        message = "Error: no se pudo agregar el archivo a la carpeta.";
        return false;
    }

    message = "Archivo " + path + " creado correctamente.";
    return true;
}


//lee el contenido de un archivo
bool FileManager::readFile(const string& path, const AppState& appState, string& content, string& message) const {
    if (!appState.session.active){
        message = "Error: debe iniciar sesion para leer archivos.";
        return false;
    }

    int inodeIndex = -1;
    Inode inode;

    if (!findInodeByPath(path, appState, inodeIndex, inode, message)){
        return false;
    }

    if (inode.i_type != '1'){
        message = "Error: la ruta indicada no corresponde a un archivo.";
        return false;
    }

    if (!canRead(inode, appState.session.uid, appState.session.gid)){
        message = "Error: no tiene permiso de lectura sobre el archivo.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (!readFileContent(mountedPartition.path, superBlock, inode, content)){
        message = "Error: no se pudo leer el contenido del archivo.";
        return false;
    }

    inode.i_atime = time(nullptr);
    if (!fileSystemManager.writeInode(mountedPartition.path, superBlock, inodeIndex, inode)){
        message = "Error: no se pudo actualizar la fecha de lectura.";
        return false;
    }

    message = "Archivo leido correctamente.";
    return true;
}


//reemplaza el contenido de un archivo
bool FileManager::writeFile(const string& path, const string& content, AppState& appState, string& message) const {
    if (!appState.session.active){
        message = "Error: debe iniciar sesion para modificar archivos.";
        return false;
    }

    int inodeIndex = -1;
    Inode inode;

    if (!findInodeByPath(path, appState, inodeIndex, inode, message)){
        return false;
    }

    if (inode.i_type != '1'){
        message = "Error: la ruta indicada no corresponde a un archivo.";
        return false;
    }

    if (!canWrite(inode, appState.session.uid, appState.session.gid)){
        message = "Error: no tiene permiso de escritura sobre el archivo.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (!writeFileContent(mountedPartition.path, mountedPartition.start, superBlock, inodeIndex, inode, content)){
        message = "Error: no se pudo escribir el contenido del archivo.";
        return false;
    }

    message = "Archivo actualizado correctamente.";
    return true;
}


//busca el inodo que corresponde a una ruta
bool FileManager::findInodeByPath(const string& path, const AppState& appState, int& inodeIndex, Inode& inode, string& message) const {
    if (!appState.session.active){
        message = "Error: no hay una sesion activa.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    if (superBlock.s_magic != 0xEF53){
        message = "Error: la particion no contiene EXT2.";
        return false;
    }

    inodeIndex = 0;

    if (!fileSystemManager.readInode(mountedPartition.path, superBlock, inodeIndex, inode)){
        message = "Error: no se pudo leer el inodo raiz.";
        return false;
    }

    vector<string> pathParts = splitPath(path);

    //si es raiz termina aqui
    if (pathParts.empty()){
        return true;
    }

    //recorre la ruta desde raiz
    for (const string& pathPart : pathParts){
        if (inode.i_type != '0'){
            message = "Error: una parte de la ruta no es una carpeta.";
            return false;
        }

        int nextInodeIndex = findEntry(mountedPartition.path, superBlock, inode, pathPart);

        if (nextInodeIndex == -1){
            message = "Error: no existe la ruta " + path + ".";
            return false;
        }

        if (!fileSystemManager.readInode(mountedPartition.path, superBlock, nextInodeIndex, inode)){
            message = "Error: no se pudo leer un inodo de la ruta.";
            return false;
        }

        inodeIndex = nextInodeIndex;
    }

    return true;
}


//obtiene las entradas de una carpeta
bool FileManager::getDirectoryContent(const string& path, const AppState& appState, vector<Content>& entries, string& message) const {
    entries.clear();

    int inodeIndex = -1;
    Inode inode;

    if (!findInodeByPath(path, appState, inodeIndex, inode, message)){
        return false;
    }

    if (inode.i_type != '0'){
        message = "Error: la ruta indicada no corresponde a una carpeta.";
        return false;
    }

    if (!canRead(inode, appState.session.uid, appState.session.gid)){
        message = "Error: no tiene permiso para leer la carpeta.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    if (!mountManager.getMountedPartition(appState, appState.session.partitionId, mountedPartition)){
        message = "Error: no se encontro la particion de la sesion.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;

    if (!fileSystemManager.readSuperBlock(mountedPartition.path, mountedPartition.start, superBlock)){
        message = "Error: no se pudo leer el superbloque.";
        return false;
    }

    //recorre bloques de la carpeta
    for (int logicalBlock = 0; logicalBlock < 4380; logicalBlock++){
        int blockIndex = getDataBlock(mountedPartition.path, superBlock, inode, logicalBlock);

        if (blockIndex == -1){
            break;
        }

        FolderBlock folderBlock;

        if (!fileSystemManager.readFolderBlock(mountedPartition.path, superBlock, blockIndex, folderBlock)){
            message = "Error: no se pudo leer un bloque de carpeta.";
            return false;
        }

        for (int entryPos = 0; entryPos < 4; entryPos++){
            if (folderBlock.b_content[entryPos].b_inodo != -1){
                entries.push_back(folderBlock.b_content[entryPos]);
            }
        }
    }

    message = "Contenido de carpeta obtenido correctamente.";
    return true;
}


//verifica si una ruta ya existe
bool FileManager::pathExists(const string& path, const AppState& appState) const {
    int inodeIndex = -1;
    Inode inode;
    string message;

    return findInodeByPath(path, appState, inodeIndex, inode, message);
}


//divide una ruta en sus partes
vector<string> FileManager::splitPath(const string& path) const {
    vector<string> parts;
    string currentPart;

    //recorre ruta y separa por /
    for (char character : path){
        if (character == '/'){
            if (!currentPart.empty()){
                parts.push_back(currentPart);
                currentPart = "";
            }

            continue;
        }

        currentPart += character;
    }

    if (!currentPart.empty()){
        parts.push_back(currentPart);
    }

    return parts;
}


//busca una entrada dentro de una carpeta
int FileManager::findEntry(const string& diskPath, const SuperBlock& superBlock, const Inode& directoryInode, const string& name) const {
    FileSystemManager fileSystemManager;

    //recorre bloques de la carpeta
    for (int logicalBlock = 0; logicalBlock < 4380; logicalBlock++){
        int blockIndex = getDataBlock(diskPath, superBlock, directoryInode, logicalBlock);

        if (blockIndex == -1){
            break;
        }

        FolderBlock folderBlock;

        if (!fileSystemManager.readFolderBlock(diskPath, superBlock, blockIndex, folderBlock)){
            return -1;
        }

        for (int entryPos = 0; entryPos < 4; entryPos++){
            if (folderBlock.b_content[entryPos].b_inodo == -1){
                continue;
            }

            string entryName = BinaryUtils::fixedCharToString(folderBlock.b_content[entryPos].b_name, 12);

            if (entryName == name){
                return folderBlock.b_content[entryPos].b_inodo;
            }
        }
    }

    return -1;
}


//busca espacio libre dentro de una carpeta
bool FileManager::findFreeDirectoryEntry(const string& diskPath, int partitionStart, SuperBlock& superBlock, int directoryInodeIndex, Inode& directoryInode, int& blockIndex, int& entryIndex) const {
    FileSystemManager fileSystemManager;

    //busca entrada libre en bloques existentes
    for (int logicalBlock = 0; logicalBlock < 4380; logicalBlock++){
        int currentBlockIndex = getDataBlock(diskPath, superBlock, directoryInode, logicalBlock);

        if (currentBlockIndex == -1){
            int currentBlocks = directoryInode.i_s / 64;
            int needed = storageBlocks(currentBlocks + 1) - storageBlocks(currentBlocks);
            if (needed > superBlock.s_free_blocks_count){
                return false;
            }
            int newBlockIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

            if (newBlockIndex == -1){
                return false;
            }

            FolderBlock newFolderBlock;

            if (!fileSystemManager.writeFolderBlock(diskPath, superBlock, newBlockIndex, newFolderBlock)){
                fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, newBlockIndex);
                return false;
            }

            if (!setDataBlock(diskPath, partitionStart, superBlock, directoryInode, logicalBlock, newBlockIndex)){
                fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, newBlockIndex);
                return false;
            }

            directoryInode.i_s += static_cast<int>(sizeof(FolderBlock));
            directoryInode.i_mtime = time(nullptr);

            if (!fileSystemManager.writeInode(diskPath, superBlock, directoryInodeIndex, directoryInode)){
                return false;
            }

            blockIndex = newBlockIndex;
            entryIndex = 0;
            return true;
        }

        FolderBlock folderBlock;

        if (!fileSystemManager.readFolderBlock(diskPath, superBlock, currentBlockIndex, folderBlock)){
            return false;
        }

        for (int entryPos = 0; entryPos < 4; entryPos++){
            if (folderBlock.b_content[entryPos].b_inodo == -1){
                blockIndex = currentBlockIndex;
                entryIndex = entryPos;
                return true;
            }
        }
    }

    return false;
}


//agrega una entrada dentro de una carpeta
bool FileManager::addDirectoryEntry(const string& diskPath, int partitionStart, SuperBlock& superBlock, int directoryInodeIndex, Inode& directoryInode, const string& name, int newInodeIndex) const {
    if (name.empty() || name.size() > 12){
        return false;
    }

    FileSystemManager fileSystemManager;
    int blockIndex = -1;
    int entryIndex = -1;

    if (!findFreeDirectoryEntry(diskPath, partitionStart, superBlock, directoryInodeIndex, directoryInode, blockIndex, entryIndex)){
        return false;
    }

    FolderBlock folderBlock;

    if (!fileSystemManager.readFolderBlock(diskPath, superBlock, blockIndex, folderBlock)){
        return false;
    }

    BinaryUtils::copyToFixedChar(folderBlock.b_content[entryIndex].b_name, 12, name);
    folderBlock.b_content[entryIndex].b_inodo = newInodeIndex;

    if (!fileSystemManager.writeFolderBlock(diskPath, superBlock, blockIndex, folderBlock)){
        return false;
    }

    directoryInode.i_mtime = time(nullptr);

    return fileSystemManager.writeInode(diskPath, superBlock, directoryInodeIndex, directoryInode);
}


//crea una carpeta individual
bool FileManager::createSingleDirectory(const string& diskPath, int partitionStart, SuperBlock& superBlock, int parentInodeIndex, const string& name, int uid, int gid, int& newInodeIndex) const {
    if (name.empty() || name.size() > 12){
        return false;
    }

    FileSystemManager fileSystemManager;
    Inode parentInode;

    if (!fileSystemManager.readInode(diskPath, superBlock, parentInodeIndex, parentInode)){
        return false;
    }

    int inodeIndex = fileSystemManager.allocateInode(diskPath, partitionStart, superBlock);

    if (inodeIndex == -1){
        return false;
    }

    int blockIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

    if (blockIndex == -1){
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, inodeIndex);
        return false;
    }

    time_t currentTime = time(nullptr);

    //crea inodo de carpeta
    Inode directoryInode;
    directoryInode.i_uid = uid;
    directoryInode.i_gid = gid;
    directoryInode.i_s = static_cast<int>(sizeof(FolderBlock));
    directoryInode.i_atime = currentTime;
    directoryInode.i_ctime = currentTime;
    directoryInode.i_mtime = currentTime;
    directoryInode.i_block[0] = blockIndex;
    directoryInode.i_type = '0';

    BinaryUtils::copyToFixedChar(directoryInode.i_perm, 3, "664");

    //crea bloque de carpeta
    FolderBlock folderBlock;

    BinaryUtils::copyToFixedChar(folderBlock.b_content[0].b_name, 12, ".");
    folderBlock.b_content[0].b_inodo = inodeIndex;

    BinaryUtils::copyToFixedChar(folderBlock.b_content[1].b_name, 12, "..");
    folderBlock.b_content[1].b_inodo = parentInodeIndex;

    if (!fileSystemManager.writeInode(diskPath, superBlock, inodeIndex, directoryInode)){
        fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, blockIndex);
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, inodeIndex);
        return false;
    }

    if (!fileSystemManager.writeFolderBlock(diskPath, superBlock, blockIndex, folderBlock)){
        fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, blockIndex);
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, inodeIndex);
        return false;
    }

    if (!addDirectoryEntry(diskPath, partitionStart, superBlock, parentInodeIndex, parentInode, name, inodeIndex)){
        fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, blockIndex);
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, inodeIndex);
        return false;
    }

    newInodeIndex = inodeIndex;
    return true;
}


//crea el inodo de un archivo
bool FileManager::createFileInode(const string& diskPath, int partitionStart, SuperBlock& superBlock, int uid, int gid, const string& content, int& inodeIndex) const {
    FileSystemManager fileSystemManager;
    int newInodeIndex = fileSystemManager.allocateInode(diskPath, partitionStart, superBlock);

    if (newInodeIndex == -1){
        return false;
    }

    time_t currentTime = time(nullptr);

    Inode inode;
    inode.i_uid = uid;
    inode.i_gid = gid;
    inode.i_s = 0;
    inode.i_atime = currentTime;
    inode.i_ctime = currentTime;
    inode.i_mtime = currentTime;
    inode.i_type = '1';

    BinaryUtils::copyToFixedChar(inode.i_perm, 3, "664");

    if (!fileSystemManager.writeInode(diskPath, superBlock, newInodeIndex, inode)){
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, newInodeIndex);
        return false;
    }

    if (!writeFileContent(diskPath, partitionStart, superBlock, newInodeIndex, inode, content)){
        fileSystemManager.freeInode(diskPath, partitionStart, superBlock, newInodeIndex);
        return false;
    }

    inodeIndex = newInodeIndex;
    return true;
}


//escribe contenido dentro de los bloques de un archivo
bool FileManager::writeFileContent(const string& diskPath, int partitionStart, SuperBlock& superBlock, int inodeIndex, Inode& inode, const string& content) const {
    FileSystemManager fileSystemManager;

    if (content.size() > 4380 * 64){
        return false;
    }
    int requiredBlocks = static_cast<int>((content.size() + 63) / 64);
    int previousBlocks = (inode.i_s + 63) / 64;
    if (storageBlocks(requiredBlocks) > superBlock.s_free_blocks_count + storageBlocks(previousBlocks)){
        return false;
    }

    //valida el espacio antes de tocar el contenido anterior
    if (!freeFileBlocks(diskPath, partitionStart, superBlock, inode)){
        return false;
    }

    size_t contentPos = 0;

    //crea bloques necesarios
    for (int logicalBlock = 0; logicalBlock < requiredBlocks; logicalBlock++){
        int blockIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (blockIndex == -1){
            freeFileBlocks(diskPath, partitionStart, superBlock, inode);
            return false;
        }

        FileBlock fileBlock;
        int charactersToCopy = static_cast<int>(content.size() - contentPos);

        if (charactersToCopy > 64){
            charactersToCopy = 64;
        }

        for (int characterPos = 0; characterPos < charactersToCopy; characterPos++){
            fileBlock.b_content[characterPos] = content[contentPos + characterPos];
        }

        if (!fileSystemManager.writeFileBlock(diskPath, superBlock, blockIndex, fileBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, blockIndex);
            freeFileBlocks(diskPath, partitionStart, superBlock, inode);
            return false;
        }

        if (!setDataBlock(diskPath, partitionStart, superBlock, inode, logicalBlock, blockIndex)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, blockIndex);
            freeFileBlocks(diskPath, partitionStart, superBlock, inode);
            return false;
        }

        contentPos += charactersToCopy;
    }

    inode.i_s = static_cast<int>(content.size());
    inode.i_mtime = time(nullptr);

    return fileSystemManager.writeInode(diskPath, superBlock, inodeIndex, inode);
}


//lee todos los bloques de un archivo
bool FileManager::readFileContent(const string& diskPath, const SuperBlock& superBlock, const Inode& inode, string& content) const {
    FileSystemManager fileSystemManager;
    content = "";

    int requiredBlocks = (inode.i_s + 63) / 64;
    int remainingCharacters = inode.i_s;

    //recorre bloques del archivo
    for (int logicalBlock = 0; logicalBlock < requiredBlocks; logicalBlock++){
        int blockIndex = getDataBlock(diskPath, superBlock, inode, logicalBlock);

        if (blockIndex == -1){
            return false;
        }

        FileBlock fileBlock;

        if (!fileSystemManager.readFileBlock(diskPath, superBlock, blockIndex, fileBlock)){
            return false;
        }

        int charactersToRead = remainingCharacters > 64 ? 64 : remainingCharacters;

        for (int characterPos = 0; characterPos < charactersToRead; characterPos++){
            content += fileBlock.b_content[characterPos];
        }

        remainingCharacters -= charactersToRead;
    }

    return true;
}


//obtiene un bloque usando los apuntadores del inodo
int FileManager::getDataBlock(const string& diskPath, const SuperBlock& superBlock, const Inode& inode, int logicalBlock) const {
    if (logicalBlock < 0){
        return -1;
    }

    //12 directos
    if (logicalBlock < 12){
        return inode.i_block[logicalBlock];
    }

    //indirecto simple
    if (logicalBlock < 28){
        return getSimpleIndirectBlock(diskPath, superBlock, inode.i_block[12], logicalBlock - 12);
    }

    //indirecto doble
    if (logicalBlock < 284){
        return getDoubleIndirectBlock(diskPath, superBlock, inode.i_block[13], logicalBlock - 28);
    }

    //indirecto triple
    if (logicalBlock < 4380){
        return getTripleIndirectBlock(diskPath, superBlock, inode.i_block[14], logicalBlock - 284);
    }

    return -1;
}


//asigna un bloque al inodo
bool FileManager::setDataBlock(const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex) const {
    if (logicalBlock < 0 || logicalBlock >= 4380){
        return false;
    }

    //12 directos
    if (logicalBlock < 12){
        inode.i_block[logicalBlock] = blockIndex;
        return true;
    }

    //indirecto simple
    if (logicalBlock < 28){
        return setSimpleIndirectBlock(diskPath, partitionStart, superBlock, inode, logicalBlock, blockIndex);
    }

    //indirecto doble
    if (logicalBlock < 284){
        return setDoubleIndirectBlock(diskPath, partitionStart, superBlock, inode, logicalBlock, blockIndex);
    }

    //indirecto triple
    return setTripleIndirectBlock(diskPath, partitionStart, superBlock, inode, logicalBlock, blockIndex);
}


//maneja apuntador indirecto simple
int FileManager::getSimpleIndirectBlock(const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position) const {
    if (pointerBlockIndex == -1 || position < 0 || position >= 16){
        return -1;
    }

    FileSystemManager fileSystemManager;
    PointerBlock pointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, pointerBlockIndex, pointerBlock)){
        return -1;
    }

    return pointerBlock.b_pointers[position];
}


//maneja apuntador indirecto doble
int FileManager::getDoubleIndirectBlock(const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position) const {
    if (pointerBlockIndex == -1 || position < 0 || position >= 256){
        return -1;
    }

    FileSystemManager fileSystemManager;
    PointerBlock firstPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, pointerBlockIndex, firstPointerBlock)){
        return -1;
    }

    int firstPosition = position / 16;
    int secondPosition = position % 16;
    int secondPointerIndex = firstPointerBlock.b_pointers[firstPosition];

    if (secondPointerIndex == -1){
        return -1;
    }

    PointerBlock secondPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
        return -1;
    }

    return secondPointerBlock.b_pointers[secondPosition];
}


//maneja apuntador indirecto triple
int FileManager::getTripleIndirectBlock(const string& diskPath, const SuperBlock& superBlock, int pointerBlockIndex, int position) const {
    if (pointerBlockIndex == -1 || position < 0 || position >= 4096){
        return -1;
    }

    FileSystemManager fileSystemManager;
    PointerBlock firstPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, pointerBlockIndex, firstPointerBlock)){
        return -1;
    }

    int firstPosition = position / 256;
    int remainingPosition = position % 256;
    int secondPosition = remainingPosition / 16;
    int thirdPosition = remainingPosition % 16;

    int secondPointerIndex = firstPointerBlock.b_pointers[firstPosition];

    if (secondPointerIndex == -1){
        return -1;
    }

    PointerBlock secondPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
        return -1;
    }

    int thirdPointerIndex = secondPointerBlock.b_pointers[secondPosition];

    if (thirdPointerIndex == -1){
        return -1;
    }

    PointerBlock thirdPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, thirdPointerIndex, thirdPointerBlock)){
        return -1;
    }

    return thirdPointerBlock.b_pointers[thirdPosition];
}


//asigna usando apuntador indirecto simple
bool FileManager::setSimpleIndirectBlock(const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex) const {
    FileSystemManager fileSystemManager;
    int position = logicalBlock - 12;

    if (position < 0 || position >= 16){
        return false;
    }

    //crea bloque de apuntadores si no existe
    if (inode.i_block[12] == -1){
        int pointerBlockIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (pointerBlockIndex == -1){
            return false;
        }

        PointerBlock pointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, pointerBlockIndex, pointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, pointerBlockIndex);
            return false;
        }

        inode.i_block[12] = pointerBlockIndex;
    }

    PointerBlock pointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[12], pointerBlock)){
        return false;
    }

    pointerBlock.b_pointers[position] = blockIndex;

    return fileSystemManager.writePointerBlock(diskPath, superBlock, inode.i_block[12], pointerBlock);
}


//asigna usando apuntador indirecto doble
bool FileManager::setDoubleIndirectBlock(const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex) const {
    FileSystemManager fileSystemManager;
    int position = logicalBlock - 28;

    if (position < 0 || position >= 256){
        return false;
    }

    int firstPosition = position / 16;
    int secondPosition = position % 16;

    //crea primer bloque de apuntadores
    if (inode.i_block[13] == -1){
        int firstPointerIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (firstPointerIndex == -1){
            return false;
        }

        PointerBlock firstPointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, firstPointerIndex, firstPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, firstPointerIndex);
            return false;
        }

        inode.i_block[13] = firstPointerIndex;
    }

    PointerBlock firstPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[13], firstPointerBlock)){
        return false;
    }

    //crea segundo bloque de apuntadores
    if (firstPointerBlock.b_pointers[firstPosition] == -1){
        int secondPointerIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (secondPointerIndex == -1){
            return false;
        }

        PointerBlock secondPointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerIndex);
            return false;
        }

        firstPointerBlock.b_pointers[firstPosition] = secondPointerIndex;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, inode.i_block[13], firstPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerIndex);
            return false;
        }
    }

    int secondPointerIndex = firstPointerBlock.b_pointers[firstPosition];
    PointerBlock secondPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
        return false;
    }

    secondPointerBlock.b_pointers[secondPosition] = blockIndex;

    return fileSystemManager.writePointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock);
}


//asigna usando apuntador indirecto triple
bool FileManager::setTripleIndirectBlock(const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode, int logicalBlock, int blockIndex) const {
    FileSystemManager fileSystemManager;
    int position = logicalBlock - 284;

    if (position < 0 || position >= 4096){
        return false;
    }

    int firstPosition = position / 256;
    int remainingPosition = position % 256;
    int secondPosition = remainingPosition / 16;
    int thirdPosition = remainingPosition % 16;

    //crea primer bloque de apuntadores
    if (inode.i_block[14] == -1){
        int firstPointerIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (firstPointerIndex == -1){
            return false;
        }

        PointerBlock firstPointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, firstPointerIndex, firstPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, firstPointerIndex);
            return false;
        }

        inode.i_block[14] = firstPointerIndex;
    }

    PointerBlock firstPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[14], firstPointerBlock)){
        return false;
    }

    //crea segundo nivel
    if (firstPointerBlock.b_pointers[firstPosition] == -1){
        int secondPointerIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (secondPointerIndex == -1){
            return false;
        }

        PointerBlock secondPointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerIndex);
            return false;
        }

        firstPointerBlock.b_pointers[firstPosition] = secondPointerIndex;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, inode.i_block[14], firstPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerIndex);
            return false;
        }
    }

    int secondPointerIndex = firstPointerBlock.b_pointers[firstPosition];
    PointerBlock secondPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
        return false;
    }

    //crea tercer nivel
    if (secondPointerBlock.b_pointers[secondPosition] == -1){
        int thirdPointerIndex = fileSystemManager.allocateBlock(diskPath, partitionStart, superBlock);

        if (thirdPointerIndex == -1){
            return false;
        }

        PointerBlock thirdPointerBlock;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, thirdPointerIndex, thirdPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, thirdPointerIndex);
            return false;
        }

        secondPointerBlock.b_pointers[secondPosition] = thirdPointerIndex;

        if (!fileSystemManager.writePointerBlock(diskPath, superBlock, secondPointerIndex, secondPointerBlock)){
            fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, thirdPointerIndex);
            return false;
        }
    }

    int thirdPointerIndex = secondPointerBlock.b_pointers[secondPosition];
    PointerBlock thirdPointerBlock;

    if (!fileSystemManager.readPointerBlock(diskPath, superBlock, thirdPointerIndex, thirdPointerBlock)){
        return false;
    }

    thirdPointerBlock.b_pointers[thirdPosition] = blockIndex;

    return fileSystemManager.writePointerBlock(diskPath, superBlock, thirdPointerIndex, thirdPointerBlock);
}


//libera los bloques usados por un archivo
bool FileManager::freeFileBlocks(const string& diskPath, int partitionStart, SuperBlock& superBlock, Inode& inode) const {
    FileSystemManager fileSystemManager;

    //libera bloques directos
    for (int blockPos = 0; blockPos < 12; blockPos++){
        if (inode.i_block[blockPos] != -1){
            if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, inode.i_block[blockPos])){
                return false;
            }

            inode.i_block[blockPos] = -1;
        }
    }

    //libera indirecto simple
    if (inode.i_block[12] != -1){
        PointerBlock pointerBlock;

        if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[12], pointerBlock)){
            return false;
        }

        for (int pointerPos = 0; pointerPos < 16; pointerPos++){
            if (pointerBlock.b_pointers[pointerPos] != -1){
                if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, pointerBlock.b_pointers[pointerPos])){
                    return false;
                }
            }
        }

        if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, inode.i_block[12])){
            return false;
        }

        inode.i_block[12] = -1;
    }

    //libera indirecto doble
    if (inode.i_block[13] != -1){
        PointerBlock firstPointerBlock;

        if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[13], firstPointerBlock)){
            return false;
        }

        for (int firstPos = 0; firstPos < 16; firstPos++){
            if (firstPointerBlock.b_pointers[firstPos] == -1){
                continue;
            }

            PointerBlock secondPointerBlock;

            if (!fileSystemManager.readPointerBlock(diskPath, superBlock, firstPointerBlock.b_pointers[firstPos], secondPointerBlock)){
                return false;
            }

            for (int secondPos = 0; secondPos < 16; secondPos++){
                if (secondPointerBlock.b_pointers[secondPos] != -1){
                    if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerBlock.b_pointers[secondPos])){
                        return false;
                    }
                }
            }

            if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, firstPointerBlock.b_pointers[firstPos])){
                return false;
            }
        }

        if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, inode.i_block[13])){
            return false;
        }

        inode.i_block[13] = -1;
    }

    //libera indirecto triple
    if (inode.i_block[14] != -1){
        PointerBlock firstPointerBlock;

        if (!fileSystemManager.readPointerBlock(diskPath, superBlock, inode.i_block[14], firstPointerBlock)){
            return false;
        }

        for (int firstPos = 0; firstPos < 16; firstPos++){
            if (firstPointerBlock.b_pointers[firstPos] == -1){
                continue;
            }

            PointerBlock secondPointerBlock;

            if (!fileSystemManager.readPointerBlock(diskPath, superBlock, firstPointerBlock.b_pointers[firstPos], secondPointerBlock)){
                return false;
            }

            for (int secondPos = 0; secondPos < 16; secondPos++){
                if (secondPointerBlock.b_pointers[secondPos] == -1){
                    continue;
                }

                PointerBlock thirdPointerBlock;

                if (!fileSystemManager.readPointerBlock(diskPath, superBlock, secondPointerBlock.b_pointers[secondPos], thirdPointerBlock)){
                    return false;
                }

                for (int thirdPos = 0; thirdPos < 16; thirdPos++){
                    if (thirdPointerBlock.b_pointers[thirdPos] != -1){
                        if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, thirdPointerBlock.b_pointers[thirdPos])){
                            return false;
                        }
                    }
                }

                if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, secondPointerBlock.b_pointers[secondPos])){
                    return false;
                }
            }

            if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, firstPointerBlock.b_pointers[firstPos])){
                return false;
            }
        }

        if (!fileSystemManager.freeBlock(diskPath, partitionStart, superBlock, inode.i_block[14])){
            return false;
        }

        inode.i_block[14] = -1;
    }

    inode.i_s = 0;

    return true;
}


//verifica permiso de lectura
bool FileManager::canRead(const Inode& inode, int uid, int gid) const {
    if (uid == 1){
        return true;
    }

    int permission = getPermission(inode, uid, gid);

    return (permission & 4) != 0;
}


//verifica permiso de escritura
bool FileManager::canWrite(const Inode& inode, int uid, int gid) const {
    if (uid == 1){
        return true;
    }

    int permission = getPermission(inode, uid, gid);

    return (permission & 2) != 0;
}


//obtiene el permiso que corresponde al usuario
int FileManager::getPermission(const Inode& inode, int uid, int gid) const {
    int permissionPosition = 2;

    //dueño
    if (inode.i_uid == uid){
        permissionPosition = 0;
    }

    //grupo
    else if (inode.i_gid == gid){
        permissionPosition = 1;
    }

    char permissionCharacter = inode.i_perm[permissionPosition];

    if (permissionCharacter < '0' || permissionCharacter > '7'){
        return 0;
    }

    return permissionCharacter - '0';
}
