#ifndef MOUNT_MANAGER_HPP
#define MOUNT_MANAGER_HPP

#include <string>
#include <vector>

#include "state/MountedPartition.hpp"

using namespace std;

struct AppState;


//maneja las particiones montadas
class MountManager {
public:

    //monta una particion primaria
    bool mountPartition(const string& path, const string& name, AppState& appState, string& message) const;

    //busca una particion montada por id
    bool getMountedPartition(const AppState& appState, const string& id, MountedPartition& mountedPartition) const;

    //devuelve las particiones montadas
    vector<MountedPartition> getMountedPartitions(const AppState& appState) const;

    //verifica si ya esta montada
    bool isMounted(const AppState& appState, const string& path, const string& name) const;

private:

    //obtiene la letra que le corresponde al disco
    char getDiskLetter(const AppState& appState, const string& path) const;

    //obtiene el siguiente correlativo del disco
    int getNextCorrelative(const AppState& appState, const string& path) const;

    //crea el id de la particion
    string createMountId(const string& carnet, int correlative, char diskLetter) const;

    //guarda los datos de montaje dentro del disco
    bool updatePartitionMount(const string& path, const string& name, int correlative, const string& id) const;
};


#endif
