#include "managers/DiskManager.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

#include "utils/BinaryUtils.hpp"
#include "utils/PathUtils.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//crea disco y escribe el mbr
bool DiskManager::createDisk(const string& path, int sizeBytes, char fit, string& message) const {
    if (sizeBytes <= static_cast<int>(sizeof(MBR))){
        message = "Error: el tamaño del disco es demasiado pequeño.";
        return false;
    }

    if (fit != 'B' && fit != 'F' && fit != 'W'){
        message = "Error: el ajuste del disco no es valido.";
        return false;
    }

    if (!validExtension(path)){
        message = "Error: el disco debe tener extension .mia.";
        return false;
    }

    if (diskExists(path)){
        message = "Error: ya existe un disco en la ruta indicada.";
        return false;
    }

    if (!PathUtils::createDirectories(path)){
        message = "Error: no se pudieron crear las carpetas para el disco.";
        return false;
    }

    ofstream file(path, ios::out | ios::binary | ios::trunc);

    if (!file.is_open()){
        message = "Error: no se pudo crear el disco.";
        return false;
    }

    char buffer[1024] = {};
    int remainingBytes = sizeBytes;

    //llena el disco completo con ceros
    while (remainingBytes > 0){
        int bytesToWrite = remainingBytes > 1024 ? 1024 : remainingBytes;

        file.write(buffer, bytesToWrite);

        if (!file.good()){
            file.close();
            remove(path.c_str());

            message = "Error: no se pudo completar la escritura del disco.";
            return false;
        }

        remainingBytes -= bytesToWrite;
    }

    file.close();


    //crea el mbr al inicio del disco
    MBR mbr;

    mbr.mbr_tamano = sizeBytes;
    mbr.mbr_fecha_creacion = time(nullptr);
    mbr.dsk_fit = fit;

    static bool randomStarted = false;

    if (!randomStarted){
        srand(static_cast<unsigned int>(time(nullptr)));
        randomStarted = true;
    }

    mbr.mbr_dsk_signature = rand();

    if (!writeMBR(path, mbr)){
        remove(path.c_str());

        message = "Error: no se pudo escribir el MBR del disco.";
        return false;
    }

    message = "Disco creado correctamente en: " + path;
    return true;
}


//elimina un disco existente
bool DiskManager::removeDisk(const string& path, string& message) const {
    if (!diskExists(path)){
        message = "Error: el disco indicado no existe.";
        return false;
    }

    if (!validExtension(path)){
        message = "Error: el archivo indicado no tiene extension .mia.";
        return false;
    }

    if (remove(path.c_str()) != 0){
        message = "Error: no se pudo eliminar el disco.";
        return false;
    }

    message = "Disco eliminado correctamente: " + path;
    return true;
}


//crea particion primaria, extendida o logica
bool DiskManager::createPartition(const string& path, int sizeBytes, char type, char fit, const string& name, string& message) const {
    if (!diskExists(path)){
        message = "Error: el disco indicado no existe.";
        return false;
    }

    if (!validExtension(path)){
        message = "Error: el archivo indicado no tiene extension .mia.";
        return false;
    }

    if (sizeBytes <= 0){
        message = "Error: el tamaño de la particion debe ser mayor a cero.";
        return false;
    }

    if (type != 'P' && type != 'E' && type != 'L'){
        message = "Error: tipo de particion no valido.";
        return false;
    }

    if (fit != 'B' && fit != 'F' && fit != 'W'){
        message = "Error: ajuste de particion no valido.";
        return false;
    }

    if (name.empty()){
        message = "Error: el nombre de la particion no puede estar vacio.";
        return false;
    }

    if (name.size() > 16){
        message = "Error: el nombre de la particion no puede superar 16 caracteres.";
        return false;
    }

    MBR mbr;

    if (!readMBR(path, mbr)){
        message = "Error: no se pudo leer el MBR del disco.";
        return false;
    }

    //el nombre no puede repetirse en todo el disco
    if (partitionNameExists(path, mbr, name)){
        message = "Error: ya existe una particion con el nombre " + name + ".";
        return false;
    }

    //las logicas se manejan dentro de la extendida
    if (type == 'L'){
        return createLogical(path, mbr, sizeBytes, fit, name, message);
    }

    return createPrimaryOrExtended(path, mbr, sizeBytes, type, fit, name, message);
}


//elimina una particion
bool DiskManager::deletePartition(const string& path, const string& name, string& message) const {
    if (!diskExists(path)){
        message = "Error: el disco indicado no existe.";
        return false;
    }

    if (!validExtension(path)){
        message = "Error: el archivo indicado no tiene extension .mia.";
        return false;
    }

    MBR mbr;

    if (!readMBR(path, mbr)){
        message = "Error: no se pudo leer el MBR del disco.";
        return false;
    }

    //primero busca primaria o extendida
    int partitionIndex = findPartition(mbr, name);

    if (partitionIndex != -1){
        return deletePrimaryOrExtended(path, mbr, partitionIndex, message);
    }

    //si no existe ahi busca una logica
    EBR logicalPartition;
    int ebrPosition = -1;

    if (findLogicalPartition(path, name, logicalPartition, ebrPosition)){
        return deleteLogical(path, name, message);
    }

    message = "Error: no existe una particion con el nombre " + name + ".";
    return false;
}


//modifica el tamaño de una particion
bool DiskManager::resizePartition(const string& path, const string& name, int addBytes, string& message) const {
    if (!diskExists(path)){
        message = "Error: el disco indicado no existe.";
        return false;
    }

    if (!validExtension(path)){
        message = "Error: el archivo indicado no tiene extension .mia.";
        return false;
    }

    if (addBytes == 0){
        message = "Error: la cantidad a modificar no puede ser cero.";
        return false;
    }

    MBR mbr;

    if (!readMBR(path, mbr)){
        message = "Error: no se pudo leer el MBR del disco.";
        return false;
    }

    //busca primaria o extendida
    int partitionIndex = findPartition(mbr, name);

    if (partitionIndex != -1){
        return resizePrimaryOrExtended(path, mbr, partitionIndex, addBytes, message);
    }

    //si no existe busca una logica
    EBR logicalPartition;
    int ebrPosition = -1;

    if (findLogicalPartition(path, name, logicalPartition, ebrPosition)){
        return resizeLogical(path, name, addBytes, message);
    }

    message = "Error: no existe una particion con el nombre " + name + ".";
    return false;
}


//lee el mbr del disco
bool DiskManager::readMBR(const string& path, MBR& mbr) const {
    if (!diskExists(path)){
        return false;
    }

    long long fileSize = BinaryUtils::getFileSize(path);

    if (fileSize < static_cast<long long>(sizeof(MBR))){
        return false;
    }

    if (!BinaryUtils::readStruct(path, 0, mbr) || mbr.mbr_tamano != fileSize){
        return false;
    }
    for (int i = 0; i < 4; i++){
        const Partition& partition = mbr.mbr_partitions[i];
        if (partition.part_s == 0) continue;
        long long end = static_cast<long long>(partition.part_start) + partition.part_s;
        if (partition.part_s < 0 || partition.part_start < static_cast<int>(sizeof(MBR)) || end > fileSize){
            return false;
        }
        for (int j = 0; j < i; j++){
            const Partition& previous = mbr.mbr_partitions[j];
            if (previous.part_s > 0 && partition.part_start < static_cast<long long>(previous.part_start) + previous.part_s && previous.part_start < end){
                return false;
            }
        }
    }
    return true;
}


//escribe el mbr al inicio del disco
bool DiskManager::writeMBR(const string& path, const MBR& mbr) const {
    return BinaryUtils::writeStruct(path, 0, mbr);
}


//busca primaria o extendida por nombre
int DiskManager::findPartition(const MBR& mbr, const string& name) const {
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        const Partition& partition = mbr.mbr_partitions[partitionPos];

        if (partition.part_s <= 0){
            continue;
        }

        string partitionName = BinaryUtils::fixedCharToString(partition.part_name, 16);

        if (StringUtils::equalsIgnoreCase(partitionName, name)){
            return partitionPos;
        }
    }

    return -1;
}


//busca una particion logica por nombre
bool DiskManager::findLogicalPartition(const string& path, const string& name, EBR& ebr, int& ebrPosition) const {
    MBR mbr;

    if (!readMBR(path, mbr)){
        return false;
    }

    int extendedIndex = findExtendedPartition(mbr);

    if (extendedIndex == -1){
        return false;
    }

    const Partition& extended = mbr.mbr_partitions[extendedIndex];

    int extendedStart = extended.part_start;
    int extendedEnd = extended.part_start + extended.part_s;
    int currentPosition = extendedStart;

    //recorre la cadena enlazada de ebr
    while (currentPosition >= extendedStart && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
        EBR currentEBR;

        if (!BinaryUtils::readStruct(path, currentPosition, currentEBR)){
            return false;
        }

        string logicalName = BinaryUtils::fixedCharToString(currentEBR.part_name, 16);

        if (currentEBR.part_s > 0 && StringUtils::equalsIgnoreCase(logicalName, name)){
            ebr = currentEBR;
            ebrPosition = currentPosition;

            return true;
        }

        if (currentEBR.part_next == -1){
            break;
        }

        if (currentEBR.part_next <= currentPosition || currentEBR.part_next >= extendedEnd){
            break;
        }

        currentPosition = currentEBR.part_next;
    }

    return false;
}


//verifica si existe el archivo
bool DiskManager::diskExists(const string& path) const {
    return BinaryUtils::fileExists(path);
}


//verifica extension .mia
bool DiskManager::validExtension(const string& path) const {
    return PathUtils::hasExtension(path, "mia");
}


//verifica nombres de primarias, extendida y logicas
bool DiskManager::partitionNameExists(const string& path, const MBR& mbr, const string& name) const {
    if (findPartition(mbr, name) != -1){
        return true;
    }

    EBR ebr;
    int ebrPosition = -1;

    return findLogicalPartition(path, name, ebr, ebrPosition);
}


//busca la particion extendida
int DiskManager::findExtendedPartition(const MBR& mbr) const {
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        const Partition& partition = mbr.mbr_partitions[partitionPos];

        if (partition.part_s > 0 && partition.part_type == 'E'){
            return partitionPos;
        }
    }

    return -1;
}


//busca una posicion libre en las cuatro entradas del mbr
int DiskManager::findFreePartitionSlot(const MBR& mbr) const {
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        if (mbr.mbr_partitions[partitionPos].part_s <= 0){
            return partitionPos;
        }
    }

    return -1;
}


//obtiene los espacios libres del disco
vector<DiskManager::FreeSpace> DiskManager::getDiskFreeSpaces(const MBR& mbr) const {
    vector<Partition> usedPartitions;
    vector<FreeSpace> freeSpaces;

    //guarda solamente las particiones que existen
    for (int partitionPos = 0; partitionPos < 4; partitionPos++){
        if (mbr.mbr_partitions[partitionPos].part_s > 0){
            usedPartitions.push_back(mbr.mbr_partitions[partitionPos]);
        }
    }

    //ordena las particiones por posicion fisica
    sort(usedPartitions.begin(), usedPartitions.end(), [](const Partition& firstPartition, const Partition& secondPartition){
        return firstPartition.part_start < secondPartition.part_start;
    });

    //el primer espacio disponible empieza despues del mbr
    int currentStart = static_cast<int>(sizeof(MBR));

    //busca espacios entre particiones
    for (const Partition& partition : usedPartitions){
        if (partition.part_start > currentStart){
            freeSpaces.push_back({currentStart, partition.part_start - currentStart});
        }

        int partitionEnd = partition.part_start + partition.part_s;

        if (partitionEnd > currentStart){
            currentStart = partitionEnd;
        }
    }

    //espacio libre al final del disco
    if (currentStart < mbr.mbr_tamano){
        freeSpaces.push_back({currentStart, mbr.mbr_tamano - currentStart});
    }

    return freeSpaces;
}


//escoge espacio segun ff, bf o wf
int DiskManager::chooseSpace(const vector<FreeSpace>& freeSpaces, int requiredSize, char fit) const {
    int selectedIndex = -1;

    //first fit
    if (fit == 'F'){
        for (size_t spacePos = 0; spacePos < freeSpaces.size(); spacePos++){
            if (freeSpaces[spacePos].size >= requiredSize){
                return static_cast<int>(spacePos);
            }
        }

        return -1;
    }

    //best fit
    if (fit == 'B'){
        for (size_t spacePos = 0; spacePos < freeSpaces.size(); spacePos++){
            if (freeSpaces[spacePos].size < requiredSize){
                continue;
            }

            if (selectedIndex == -1 || freeSpaces[spacePos].size < freeSpaces[selectedIndex].size){
                selectedIndex = static_cast<int>(spacePos);
            }
        }

        return selectedIndex;
    }

    //worst fit
    if (fit == 'W'){
        for (size_t spacePos = 0; spacePos < freeSpaces.size(); spacePos++){
            if (freeSpaces[spacePos].size < requiredSize){
                continue;
            }

            if (selectedIndex == -1 || freeSpaces[spacePos].size > freeSpaces[selectedIndex].size){
                selectedIndex = static_cast<int>(spacePos);
            }
        }

        return selectedIndex;
    }

    return -1;
}


//crea primaria o extendida
bool DiskManager::createPrimaryOrExtended(const string& path, MBR& mbr, int sizeBytes, char type, char fit, const string& name, string& message) const {
    int freeSlot = findFreePartitionSlot(mbr);

    //primarias + extendida maximo 4
    if (freeSlot == -1){
        message = "Error: el disco ya tiene el maximo de 4 particiones primarias o extendidas.";
        return false;
    }

    //solo puede existir una extendida
    if (type == 'E' && findExtendedPartition(mbr) != -1){
        message = "Error: el disco ya contiene una particion extendida.";
        return false;
    }

    //la extendida debe guardar por lo menos el primer ebr
    if (type == 'E' && sizeBytes <= static_cast<int>(sizeof(EBR))){
        message = "Error: la particion extendida es demasiado pequeña para almacenar un EBR.";
        return false;
    }

    vector<FreeSpace> freeSpaces = getDiskFreeSpaces(mbr);

    //para ubicar particiones usa el ajuste general del disco
    int selectedSpace = chooseSpace(freeSpaces, sizeBytes, mbr.dsk_fit);

    if (selectedSpace == -1){
        message = "Error: no existe suficiente espacio disponible para crear la particion.";
        return false;
    }

    Partition partition;

    partition.part_status = '0';
    partition.part_type = type;
    partition.part_fit = fit;
    partition.part_start = freeSpaces[selectedSpace].start;
    partition.part_s = sizeBytes;
    partition.part_correlative = -1;

    BinaryUtils::copyToFixedChar(partition.part_name, 16, name);
    BinaryUtils::copyToFixedChar(partition.part_id, 4, "");

    mbr.mbr_partitions[freeSlot] = partition;

    //solo se escribe el objeto partition dentro del mbr
    if (!writeMBR(path, mbr)){
        message = "Error: no se pudo guardar la nueva particion en el MBR.";
        return false;
    }

    //al crear una extendida se crea su primer ebr
    if (type == 'E'){
        if (!createFirstEBR(path, partition)){
            mbr.mbr_partitions[freeSlot] = Partition();
            writeMBR(path, mbr);

            message = "Error: no se pudo crear el primer EBR de la particion extendida.";
            return false;
        }
    }

    message = "Particion " + name + " creada correctamente.";
    return true;
}


//crea una particion logica dentro de la extendida
bool DiskManager::createLogical(const string& path, MBR& mbr, int sizeBytes, char fit, const string& name, string& message) const {
    int extendedIndex = findExtendedPartition(mbr);

    if (extendedIndex == -1){
        message = "Error: no se puede crear una particion logica sin una particion extendida.";
        return false;
    }

    const Partition& extended = mbr.mbr_partitions[extendedIndex];

    int extendedStart = extended.part_start;
    int extendedEnd = extended.part_start + extended.part_s;

    //cada logica necesita su ebr y el espacio logico
    if (sizeBytes > extended.part_s - static_cast<int>(sizeof(EBR))){
        message = "Error: la particion logica supera el espacio de la extendida.";
        return false;
    }
    int requiredSize = static_cast<int>(sizeof(EBR)) + sizeBytes;

    if (requiredSize > extended.part_s){
        message = "Error: la particion logica supera el tamaño de la particion extendida.";
        return false;
    }

    vector<int> ebrPositions;
    vector<EBR> ebrs;

    int currentPosition = extendedStart;

    //lee todos los ebr enlazados
    while (currentPosition >= extendedStart && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
        EBR currentEBR;

        if (!BinaryUtils::readStruct(path, currentPosition, currentEBR)){
            message = "Error: no se pudo leer la cadena de EBR.";
            return false;
        }

        ebrPositions.push_back(currentPosition);
        ebrs.push_back(currentEBR);

        if (currentEBR.part_next == -1){
            break;
        }

        if (currentEBR.part_next <= currentPosition || currentEBR.part_next >= extendedEnd){
            message = "Error: la cadena de EBR contiene una posicion invalida.";
            return false;
        }

        currentPosition = currentEBR.part_next;
    }

    if (ebrs.empty()){
        message = "Error: la particion extendida no contiene un EBR inicial.";
        return false;
    }

    vector<FreeSpace> freeSpaces;

    //si el primer ebr esta vacio puede reutilizarse
    if (ebrs[0].part_s <= 0){
        int nextPosition = ebrs[0].part_next == -1 ? extendedEnd : ebrs[0].part_next;
        int availableSize = nextPosition - extendedStart;

        if (availableSize >= requiredSize){
            freeSpaces.push_back({extendedStart, availableSize});
        }
    }

    //busca huecos despues de cada logica existente
    for (size_t ebrPos = 0; ebrPos < ebrs.size(); ebrPos++){
        //el primer ebr vacio ya fue agregado arriba
        if (ebrPos == 0 && ebrs[ebrPos].part_s <= 0){
            continue;
        }

        if (ebrs[ebrPos].part_s <= 0){
            continue;
        }

        int logicalEnd = ebrs[ebrPos].part_start + ebrs[ebrPos].part_s;
        int nextPosition = ebrs[ebrPos].part_next == -1 ? extendedEnd : ebrs[ebrPos].part_next;

        if (nextPosition > logicalEnd && nextPosition - logicalEnd >= requiredSize){
            freeSpaces.push_back({logicalEnd, nextPosition - logicalEnd});
        }
    }

    int selectedSpace = chooseSpace(freeSpaces, requiredSize, fit);

    if (selectedSpace == -1){
        message = "Error: no existe suficiente espacio dentro de la particion extendida.";
        return false;
    }

    int newEBRPosition = freeSpaces[selectedSpace].start;

    EBR newEBR;

    newEBR.part_mount = '0';
    newEBR.part_fit = fit;
    newEBR.part_start = newEBRPosition + static_cast<int>(sizeof(EBR));
    newEBR.part_s = sizeBytes;
    newEBR.part_next = -1;

    BinaryUtils::copyToFixedChar(newEBR.part_name, 16, name);


    //reutiliza el ebr inicial si estaba vacio
    if (newEBRPosition == extendedStart && ebrs[0].part_s <= 0){
        newEBR.part_next = ebrs[0].part_next;

        if (!BinaryUtils::writeStruct(path, extendedStart, newEBR)){
            message = "Error: no se pudo escribir la particion logica.";
            return false;
        }

        message = "Particion logica " + name + " creada correctamente.";
        return true;
    }


    int previousIndex = -1;

    //busca el ebr que debe apuntar hacia el nuevo
    for (size_t ebrPos = 0; ebrPos < ebrPositions.size(); ebrPos++){
        int nextPosition = ebrs[ebrPos].part_next;

        bool newIsAfterCurrent = ebrPositions[ebrPos] < newEBRPosition;
        bool beforeNext = nextPosition == -1 || newEBRPosition < nextPosition;

        if (newIsAfterCurrent && beforeNext){
            previousIndex = static_cast<int>(ebrPos);
            break;
        }
    }

    if (previousIndex == -1){
        message = "Error: no se pudo enlazar la nueva particion logica.";
        return false;
    }

    //el nuevo apunta al que antes seguia
    newEBR.part_next = ebrs[previousIndex].part_next;

    //el anterior ahora apunta al nuevo
    EBR previousEBR = ebrs[previousIndex];
    previousEBR.part_next = newEBRPosition;

    //primero escribe el nuevo ebr
    if (!BinaryUtils::writeStruct(path, newEBRPosition, newEBR)){
        message = "Error: no se pudo escribir la nueva particion logica.";
        return false;
    }

    //despues actualiza el enlace anterior
    if (!BinaryUtils::writeStruct(path, ebrPositions[previousIndex], previousEBR)){
        clearSpace(path, newEBRPosition, requiredSize);

        message = "Error: no se pudo actualizar el EBR anterior.";
        return false;
    }

    message = "Particion logica " + name + " creada correctamente.";
    return true;
}


//crea el primer ebr de una extendida
bool DiskManager::createFirstEBR(const string& path, const Partition& extendedPartition) const {
    EBR ebr;

    ebr.part_mount = '0';
    ebr.part_fit = extendedPartition.part_fit;

    //el ebr esta al inicio y la parte logica iria despues
    ebr.part_start = extendedPartition.part_start + static_cast<int>(sizeof(EBR));

    //todavia no existe una logica
    ebr.part_s = 0;

    //no existe siguiente ebr
    ebr.part_next = -1;

    BinaryUtils::copyToFixedChar(ebr.part_name, 16, "");

    return BinaryUtils::writeStruct(path, extendedPartition.part_start, ebr);
}


//elimina primaria o extendida
bool DiskManager::deletePrimaryOrExtended(const string& path, MBR& mbr, int partitionIndex, string& message) const {
    if (partitionIndex < 0 || partitionIndex >= 4){
        message = "Error: posicion de particion no valida.";
        return false;
    }

    Partition partition = mbr.mbr_partitions[partitionIndex];
    string partitionName = BinaryUtils::fixedCharToString(partition.part_name, 16);

    if (partition.part_s <= 0){
        message = "Error: la particion no existe.";
        return false;
    }

    //una particion montada no se elimina
    if (partition.part_status != '0'){
        message = "Error: no se puede eliminar una particion montada.";
        return false;
    }

    //elimina sus datos de la region reservada
    if (!clearSpace(path, partition.part_start, partition.part_s)){
        message = "Error: no se pudo limpiar el espacio de la particion.";
        return false;
    }

    //el objeto partition desaparece del mbr
    mbr.mbr_partitions[partitionIndex] = Partition();

    if (!writeMBR(path, mbr)){
        message = "Error: no se pudo actualizar el MBR.";
        return false;
    }

    message = "Particion " + partitionName + " eliminada correctamente.";
    return true;
}


//elimina una particion logica
bool DiskManager::deleteLogical(const string& path, const string& name, string& message) const {
    MBR mbr;

    if (!readMBR(path, mbr)){
        message = "Error: no se pudo leer el MBR del disco.";
        return false;
    }

    int extendedIndex = findExtendedPartition(mbr);

    if (extendedIndex == -1){
        message = "Error: el disco no contiene una particion extendida.";
        return false;
    }

    const Partition& extended = mbr.mbr_partitions[extendedIndex];

    int extendedStart = extended.part_start;
    int extendedEnd = extended.part_start + extended.part_s;

    int currentPosition = extendedStart;
    int previousPosition = -1;

    EBR previousEBR;


    //recorre la cadena de ebr
    while (currentPosition >= extendedStart && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
        EBR currentEBR;

        if (!BinaryUtils::readStruct(path, currentPosition, currentEBR)){
            message = "Error: no se pudo leer la cadena de EBR.";
            return false;
        }

        string currentName = BinaryUtils::fixedCharToString(currentEBR.part_name, 16);


        if (currentEBR.part_s > 0 && StringUtils::equalsIgnoreCase(currentName, name)){
            if (currentEBR.part_mount != '0'){
                message = "Error: no se puede eliminar una particion logica montada.";
                return false;
            }


            //si es la primera deja el ebr inicial vacio
            if (currentPosition == extendedStart){
                if (!clearSpace(path, currentEBR.part_start, currentEBR.part_s)){
                    message = "Error: no se pudo limpiar la particion logica.";
                    return false;
                }

                EBR emptyEBR;

                emptyEBR.part_mount = '0';
                emptyEBR.part_fit = extended.part_fit;
                emptyEBR.part_start = extendedStart + static_cast<int>(sizeof(EBR));
                emptyEBR.part_s = 0;
                emptyEBR.part_next = currentEBR.part_next;

                BinaryUtils::copyToFixedChar(emptyEBR.part_name, 16, "");

                if (!BinaryUtils::writeStruct(path, extendedStart, emptyEBR)){
                    message = "Error: no se pudo actualizar el EBR.";
                    return false;
                }

                message = "Particion logica " + name + " eliminada correctamente.";
                return true;
            }


            //si no es la primera, el anterior apunta al siguiente
            EBR updatedPrevious = previousEBR;
            updatedPrevious.part_next = currentEBR.part_next;

            if (!BinaryUtils::writeStruct(path, previousPosition, updatedPrevious)){
                message = "Error: no se pudo actualizar la cadena de EBR.";
                return false;
            }

            int logicalEnd = currentEBR.part_start + currentEBR.part_s;
            int clearSize = logicalEnd - currentPosition;

            //borra ebr y espacio de la logica
            if (!clearSpace(path, currentPosition, clearSize)){
                BinaryUtils::writeStruct(path, previousPosition, previousEBR);

                message = "Error: no se pudo limpiar la particion logica.";
                return false;
            }

            message = "Particion logica " + name + " eliminada correctamente.";
            return true;
        }


        if (currentEBR.part_next == -1){
            break;
        }

        if (currentEBR.part_next <= currentPosition || currentEBR.part_next >= extendedEnd){
            break;
        }

        previousPosition = currentPosition;
        previousEBR = currentEBR;

        currentPosition = currentEBR.part_next;
    }

    message = "Error: no existe una particion logica con el nombre " + name + ".";
    return false;
}


//modifica primaria o extendida
bool DiskManager::resizePrimaryOrExtended(const string& path, MBR& mbr, int partitionIndex, int addBytes, string& message) const {
    if (partitionIndex < 0 || partitionIndex >= 4){
        message = "Error: posicion de particion no valida.";
        return false;
    }

    Partition& partition = mbr.mbr_partitions[partitionIndex];

    if (partition.part_s <= 0){
        message = "Error: la particion no existe.";
        return false;
    }

    if (partition.part_status != '0'){
        message = "Error: no se puede modificar una particion montada.";
        return false;
    }

    string partitionName = BinaryUtils::fixedCharToString(partition.part_name, 16);


    //aumentar
    if (addBytes > 0){
        long long partitionEnd = static_cast<long long>(partition.part_start) + partition.part_s;
        long long nextStart = mbr.mbr_tamano;

        //busca la siguiente particion fisica
        for (int currentIndex = 0; currentIndex < 4; currentIndex++){
            if (currentIndex == partitionIndex){
                continue;
            }

            const Partition& currentPartition = mbr.mbr_partitions[currentIndex];

            if (currentPartition.part_s <= 0){
                continue;
            }

            if (currentPartition.part_start > partition.part_start && currentPartition.part_start < nextStart){
                nextStart = currentPartition.part_start;
            }
        }

        long long availableSpace = nextStart - partitionEnd;

        if (availableSpace < addBytes){
            message = "Error: no existe suficiente espacio continuo para aumentar la particion.";
            return false;
        }

        long long newSize = static_cast<long long>(partition.part_s) + addBytes;

        if (newSize <= 0 || newSize > 2147483647LL){
            message = "Error: el nuevo tamaño de la particion no es valido.";
            return false;
        }

        partition.part_s = static_cast<int>(newSize);

        if (!writeMBR(path, mbr)){
            partition.part_s -= addBytes;

            message = "Error: no se pudo actualizar el tamaño de la particion.";
            return false;
        }

        message = "Particion " + partitionName + " aumentada correctamente.";
        return true;
    }


    //reducir
    long long removeBytes = -(static_cast<long long>(addBytes));
    long long newSize = static_cast<long long>(partition.part_s) - removeBytes;

    if (newSize <= 0){
        message = "Error: la reduccion dejaria la particion con tamaño invalido.";
        return false;
    }


    //una extendida no puede cortar sus ebr o logicas
    if (partition.part_type == 'E'){
        int extendedStart = partition.part_start;
        int extendedEnd = partition.part_start + partition.part_s;

        int minimumEnd = extendedStart + static_cast<int>(sizeof(EBR));
        int currentPosition = extendedStart;

        while (currentPosition >= extendedStart && currentPosition + static_cast<int>(sizeof(EBR)) <= extendedEnd){
            EBR currentEBR;

            if (!BinaryUtils::readStruct(path, currentPosition, currentEBR)){
                message = "Error: no se pudo leer la cadena de EBR.";
                return false;
            }

            //el ebr tambien ocupa espacio
            int ebrEnd = currentPosition + static_cast<int>(sizeof(EBR));

            if (ebrEnd > minimumEnd){
                minimumEnd = ebrEnd;
            }

            //si tiene logica toma en cuenta su final
            if (currentEBR.part_s > 0){
                int logicalEnd = currentEBR.part_start + currentEBR.part_s;

                if (logicalEnd > minimumEnd){
                    minimumEnd = logicalEnd;
                }
            }

            if (currentEBR.part_next == -1){
                break;
            }

            if (currentEBR.part_next <= currentPosition || currentEBR.part_next >= extendedEnd){
                message = "Error: la cadena de EBR contiene una posicion invalida.";
                return false;
            }

            currentPosition = currentEBR.part_next;
        }

        long long newEnd = static_cast<long long>(partition.part_start) + newSize;

        if (newEnd < minimumEnd){
            message = "Error: no se puede reducir la extendida porque contiene EBR o particiones logicas en ese espacio.";
            return false;
        }
    }


    long long oldEnd = static_cast<long long>(partition.part_start) + partition.part_s;
    long long newEnd = static_cast<long long>(partition.part_start) + newSize;
    int releasedSize = static_cast<int>(oldEnd - newEnd);

    //limpia la parte liberada
    if (!clearSpace(path, static_cast<int>(newEnd), releasedSize)){
        message = "Error: no se pudo limpiar el espacio reducido.";
        return false;
    }

    int oldSize = partition.part_s;

    partition.part_s = static_cast<int>(newSize);

    if (!writeMBR(path, mbr)){
        partition.part_s = oldSize;

        message = "Error: no se pudo actualizar el tamaño de la particion.";
        return false;
    }

    message = "Particion " + partitionName + " reducida correctamente.";
    return true;
}


//modifica una particion logica
bool DiskManager::resizeLogical(const string& path, const string& name, int addBytes, string& message) const {
    MBR mbr;

    if (!readMBR(path, mbr)){
        message = "Error: no se pudo leer el MBR del disco.";
        return false;
    }

    int extendedIndex = findExtendedPartition(mbr);

    if (extendedIndex == -1){
        message = "Error: el disco no contiene una particion extendida.";
        return false;
    }

    const Partition& extended = mbr.mbr_partitions[extendedIndex];

    int extendedEnd = extended.part_start + extended.part_s;

    EBR logicalPartition;
    int ebrPosition = -1;

    if (!findLogicalPartition(path, name, logicalPartition, ebrPosition)){
        message = "Error: no existe la particion logica " + name + ".";
        return false;
    }

    if (logicalPartition.part_mount != '0'){
        message = "Error: no se puede modificar una particion logica montada.";
        return false;
    }


    //aumentar
    if (addBytes > 0){
        long long logicalEnd = static_cast<long long>(logicalPartition.part_start) + logicalPartition.part_s;

        long long nextStart = extendedEnd;

        //el siguiente ebr limita hasta donde puede crecer
        if (logicalPartition.part_next != -1){
            nextStart = logicalPartition.part_next;
        }

        long long availableSpace = nextStart - logicalEnd;

        if (availableSpace < addBytes){
            message = "Error: no existe suficiente espacio continuo para aumentar la particion logica.";
            return false;
        }

        long long newSize = static_cast<long long>(logicalPartition.part_s) + addBytes;

        if (newSize <= 0 || newSize > 2147483647LL){
            message = "Error: el nuevo tamaño de la particion logica no es valido.";
            return false;
        }

        logicalPartition.part_s = static_cast<int>(newSize);

        if (!BinaryUtils::writeStruct(path, ebrPosition, logicalPartition)){
            message = "Error: no se pudo actualizar la particion logica.";
            return false;
        }

        message = "Particion logica " + name + " aumentada correctamente.";
        return true;
    }


    //reducir
    long long removeBytes = -(static_cast<long long>(addBytes));
    long long newSize = static_cast<long long>(logicalPartition.part_s) - removeBytes;

    if (newSize <= 0){
        message = "Error: la reduccion dejaria la particion logica con tamaño invalido.";
        return false;
    }

    long long oldEnd = static_cast<long long>(logicalPartition.part_start) + logicalPartition.part_s;
    long long newEnd = static_cast<long long>(logicalPartition.part_start) + newSize;

    int releasedSize = static_cast<int>(oldEnd - newEnd);

    //limpia la parte liberada
    if (!clearSpace(path, static_cast<int>(newEnd), releasedSize)){
        message = "Error: no se pudo limpiar el espacio reducido de la particion logica.";
        return false;
    }

    int oldSize = logicalPartition.part_s;

    logicalPartition.part_s = static_cast<int>(newSize);

    if (!BinaryUtils::writeStruct(path, ebrPosition, logicalPartition)){
        logicalPartition.part_s = oldSize;

        message = "Error: no se pudo actualizar la particion logica.";
        return false;
    }

    message = "Particion logica " + name + " reducida correctamente.";
    return true;
}


//llena una parte del disco con ceros
bool DiskManager::clearSpace(const string& path, int start, int size) const {
    if (start < 0 || size < 0){
        return false;
    }

    if (size == 0){
        return true;
    }

    return BinaryUtils::clearSpace(path, start, size);
}
