#ifndef DISK_STRUCTURES_HPP
#define DISK_STRUCTURES_HPP

#include <ctime>

using namespace std;


//info de una particion primaria o extendida
struct Partition {
    char part_status = '0';
    char part_type = '0';
    char part_fit = '0';
    int part_start = -1;
    int part_s = 0;
    char part_name[16] = {};
    int part_correlative = -1;
    char part_id[4] = {};
};


//info principal del disco
struct MBR {
    int mbr_tamano = 0;
    time_t mbr_fecha_creacion = 0;
    int mbr_dsk_signature = 0;
    char dsk_fit = 'F';
    Partition mbr_partitions[4];
};


//info de una particion logica
struct EBR {
    char part_mount = '0';
    char part_fit = '0';
    int part_start = -1;
    int part_s = 0;
    int part_next = -1;
    char part_name[16] = {};
};


#endif
