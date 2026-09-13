#ifndef MOUNTED_PARTITION_HPP
#define MOUNTED_PARTITION_HPP

#include <string>

using namespace std;


//guarda una particion montada en memoria
struct MountedPartition {
    string id = "";
    string path = "";
    string name = "";
    int start = -1;
    int size = 0;
    int correlative = -1;
    char diskLetter = 'A';
};


#endif
