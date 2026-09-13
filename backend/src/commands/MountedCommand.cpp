#include "commands/MountedCommand.hpp"

using namespace std;


//ejecuta mounted
ValidationResult MountedCommand::execute(const ParsedCommand& command, const AppState& appState) const {

    //mounted no recibe parametros
    if (!command.params.empty()){
        return {false, "MOUNTED: este comando no recibe parametros."};
    }

    //verifica si hay particiones montadas
    if (appState.mountedPartitions.empty()){
        return {true, "No hay particiones montadas."};
    }

    string message = "Particiones montadas:\n";

    //recorre las particiones montadas
    for (const MountedPartition& mountedPartition : appState.mountedPartitions){
        message += "ID: " + mountedPartition.id;
        message += " | Nombre: " + mountedPartition.name;
        message += " | Disco: " + mountedPartition.path;
        message += "\n";
    }

    return {true, message};
}
