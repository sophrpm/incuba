#ifndef DISK_MANAGER_HPP
#define DISK_MANAGER_HPP

#include <string>
#include <vector>

#include "structures/DiskStructures.hpp"

using namespace std;


//maneja los discos .mia y sus particiones
class DiskManager {
public:

    bool createDisk(const string& path, int sizeBytes, char fit, string& message) const;

    bool removeDisk(const string& path, string& message) const;

    bool createPartition(const string& path, int sizeBytes, char type, char fit, const string& name, string& message) const;

    bool deletePartition(const string& path, const string& name, string& message) const;

    //aumenta o reduce tamaño de una particion
    bool resizePartition(const string& path, const string& name, int addBytes, string& message) const;

    bool readMBR(const string& path, MBR& mbr) const;

    bool writeMBR(const string& path, const MBR& mbr) const;

    int findPartition(const MBR& mbr, const string& name) const;

    bool findLogicalPartition(const string& path, const string& name, EBR& ebr, int& ebrPosition) const;


private:

    struct FreeSpace {
        int start;
        int size;
    };

    bool diskExists(const string& path) const;

    bool validExtension(const string& path) const;

    bool partitionNameExists(const string& path, const MBR& mbr, const string& name) const;

    int findExtendedPartition(const MBR& mbr) const;

    int findFreePartitionSlot(const MBR& mbr) const;

    vector<FreeSpace> getDiskFreeSpaces(const MBR& mbr) const;

    int chooseSpace(const vector<FreeSpace>& freeSpaces, int requiredSize, char fit) const;

    bool createPrimaryOrExtended(const string& path, MBR& mbr, int sizeBytes, char type, char fit, const string& name, string& message) const;

    bool createLogical(const string& path, MBR& mbr, int sizeBytes, char fit, const string& name, string& message) const;

    bool createFirstEBR(const string& path, const Partition& extendedPartition) const;

    bool deletePrimaryOrExtended(const string& path, MBR& mbr, int partitionIndex, string& message) const;

    bool deleteLogical(const string& path, const string& name, string& message) const;

    //modifica primaria o extendida
    bool resizePrimaryOrExtended(const string& path, MBR& mbr, int partitionIndex, int addBytes, string& message) const;

    //modifica una particion logica
    bool resizeLogical(const string& path, const string& name, int addBytes, string& message) const;

    bool clearSpace(const string& path, int start, int size) const;
};


#endif
