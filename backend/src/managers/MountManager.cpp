#include "managers/MountManager.hpp"

#include "managers/DiskManager.hpp"
#include "managers/FileSystemManager.hpp"
#include "utils/PathUtils.hpp"
#include "state/AppState.hpp"
#include "utils/BinaryUtils.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//monta una particion primaria
bool MountManager::mountPartition(const string& inputPath, const string& name, AppState& appState, string& message) const {
    string path = PathUtils::canonicalPath(inputPath);
    DiskManager diskManager;
    MBR mbr;

    //verifica que se pueda leer el disco
    if (!diskManager.readMBR(path, mbr)){
        message = "Error: no se pudo leer el disco indicado.";
        return false;
    }

    //busca la particion en el mbr
    int partitionIndex = diskManager.findPartition(mbr, name);

    if (partitionIndex == -1){
        message = "Error: no existe una particion primaria con el nombre " + name + ".";
        return false;
    }

    Partition partition = mbr.mbr_partitions[partitionIndex];

    //para el proyecto solo se montan primarias
    if (partition.part_type != 'P'){
        message = "Error: solamente se pueden montar particiones primarias.";
        return false;
    }

    //verifica si ya esta montada
    if (isMounted(appState, path, name)){
        message = "Error: la particion ya se encuentra montada.";
        return false;
    }

    char diskLetter = getDiskLetter(appState, path);

    if (diskLetter == '\0'){
        message = "Error: no hay letras disponibles para montar otro disco.";
        return false;
    }

    int correlative = getNextCorrelative(appState, path);
    string id = createMountId(appState.carnet, correlative, diskLetter);

    if (id.empty()){
        message = "Error: no se pudo generar el id de la particion.";
        return false;
    }

    //actualiza status, correlativo e id en el mbr
    if (!updatePartitionMount(path, name, correlative, id)){
        message = "Error: no se pudo actualizar la particion dentro del disco.";
        return false;
    }

    FileSystemManager fileSystemManager;
    SuperBlock superBlock;
    if (fileSystemManager.readSuperBlock(path, partition.part_start, superBlock) && superBlock.s_magic == 0xEF53){
        superBlock.s_mtime = time(nullptr);
        superBlock.s_mnt_count++;
        if (!fileSystemManager.writeSuperBlock(path, partition.part_start, superBlock)){
            message = "Error: no se pudo actualizar la fecha del montaje.";
            return false;
        }
    }

    //guarda montaje en memoria
    MountedPartition mountedPartition;
    mountedPartition.id = id;
    mountedPartition.path = path;
    mountedPartition.name = BinaryUtils::fixedCharToString(partition.part_name, 16);
    mountedPartition.start = partition.part_start;
    mountedPartition.size = partition.part_s;
    mountedPartition.correlative = correlative;
    mountedPartition.diskLetter = diskLetter;

    appState.mountedPartitions.push_back(mountedPartition);

    message = "Particion " + mountedPartition.name + " montada correctamente con id " + id + ".";
    return true;
}


//busca una particion montada por id
bool MountManager::getMountedPartition(const AppState& appState, const string& id, MountedPartition& mountedPartition) const {
    for (const MountedPartition& currentPartition : appState.mountedPartitions){
        if (currentPartition.id == id){
            DiskManager diskManager;
            MBR mbr;
            if (!diskManager.readMBR(currentPartition.path, mbr)){
                return false;
            }
            int index = diskManager.findPartition(mbr, currentPartition.name);
            if (index < 0 || mbr.mbr_partitions[index].part_start != currentPartition.start || mbr.mbr_partitions[index].part_s != currentPartition.size){
                return false;
            }
            mountedPartition = currentPartition;
            return true;
        }
    }

    return false;
}


//devuelve las particiones montadas
vector<MountedPartition> MountManager::getMountedPartitions(const AppState& appState) const {
    return appState.mountedPartitions;
}


//verifica si ya esta montada
bool MountManager::isMounted(const AppState& appState, const string& path, const string& name) const {
    for (const MountedPartition& mountedPartition : appState.mountedPartitions){
        if (mountedPartition.path == path && StringUtils::equalsIgnoreCase(mountedPartition.name, name)){
            return true;
        }
    }

    return false;
}


//obtiene la letra que le corresponde al disco
char MountManager::getDiskLetter(const AppState& appState, const string& path) const {

    //si el disco ya tiene montajes conserva su letra
    for (const MountedPartition& mountedPartition : appState.mountedPartitions){
        if (mountedPartition.path == path){
            return mountedPartition.diskLetter;
        }
    }

    //busca la primera letra disponible
    for (char letter = 'A'; letter <= 'Z'; letter++){
        bool letterUsed = false;

        for (const MountedPartition& mountedPartition : appState.mountedPartitions){
            if (mountedPartition.diskLetter == letter){
                letterUsed = true;
                break;
            }
        }

        if (!letterUsed){
            return letter;
        }
    }

    return '\0';
}


//obtiene el siguiente correlativo del disco
int MountManager::getNextCorrelative(const AppState& appState, const string& path) const {
    int greaterCorrelative = 0;

    //busca mayor correlativo del mismo disco
    for (const MountedPartition& mountedPartition : appState.mountedPartitions){
        if (mountedPartition.path == path && mountedPartition.correlative > greaterCorrelative){
            greaterCorrelative = mountedPartition.correlative;
        }
    }

    return greaterCorrelative + 1;
}


//crea el id de la particion
string MountManager::createMountId(const string& carnet, int correlative, char diskLetter) const {
    if (carnet.size() < 2){
        return "";
    }

    if (correlative < 1 || correlative > 9){
        return "";
    }

    if (diskLetter < 'A' || diskLetter > 'Z'){
        return "";
    }

    string carnetEnd = carnet.substr(carnet.size() - 2);

    return carnetEnd + to_string(correlative) + diskLetter;
}


//guarda datos del montaje dentro del mbr
bool MountManager::updatePartitionMount(const string& path, const string& name, int correlative, const string& id) const {
    DiskManager diskManager;
    MBR mbr;

    if (!diskManager.readMBR(path, mbr)){
        return false;
    }

    int partitionIndex = diskManager.findPartition(mbr, name);

    if (partitionIndex == -1){
        return false;
    }

    Partition& partition = mbr.mbr_partitions[partitionIndex];

    //solo primarias
    if (partition.part_type != 'P'){
        return false;
    }

    partition.part_status = '1';
    partition.part_correlative = correlative;

    //part_id tiene exactamente 4 posiciones
    BinaryUtils::copyToFixedChar(partition.part_id, 4, id);

    return diskManager.writeMBR(path, mbr);
}
