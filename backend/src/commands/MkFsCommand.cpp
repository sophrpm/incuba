#include "commands/MkFsCommand.hpp"

#include "managers/FileSystemManager.hpp"
#include "managers/MountManager.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//ejecuta mkfs
ValidationResult MkFsCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "id" && param.name != "type" && param.name != "fs"){
            return {false, "MKFS: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("id") > 1){
        return {false, "MKFS: el parametro -id esta repetido."};
    }

    if (command.countParam("type") > 1){
        return {false, "MKFS: el parametro -type esta repetido."};
    }

    //id es obligatorio
    if (!command.hasParam("id")){
        return {false, "MKFS: falta el parametro obligatorio -id."};
    }

    if (!command.paramHasValue("id")){
        return {false, "MKFS: el parametro -id necesita un valor."};
    }

    string id = command.getParam("id");

    if (id.empty()){
        return {false, "MKFS: el valor de -id no puede estar vacio."};
    }

    //valores por defecto
    string type = "full";

    if (command.hasParam("type")){
        if (!command.paramHasValue("type")){
            return {false, "MKFS: el parametro -type necesita un valor."};
        }

        type = StringUtils::toLower(command.getParam("type"));
    }

    //solo soportamos formato full
    if (type != "full"){
        return {false, "MKFS: -type solo acepta full."};
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    //busca la particion montada
    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        return {false, "MKFS: no existe una particion montada con el id " + id + "."};
    }

    FileSystemManager fileSystemManager;
    string message;

    if (command.countParam("fs") > 1 || (command.hasParam("fs") && !command.paramHasValue("fs"))) return {false, "MKFS: parametro -fs invalido."};
    string fs = command.hasParam("fs") ? StringUtils::toLower(command.getParam("fs")) : "2fs";
    if (fs != "2fs" && fs != "3fs") return {false, "MKFS: -fs acepta 2fs o 3fs."};
    //formatea la particion
    bool success = fs == "3fs" ? fileSystemManager.formatExt3(mountedPartition.path, mountedPartition.start, mountedPartition.size, message) : fileSystemManager.formatExt2(mountedPartition.path, mountedPartition.start, mountedPartition.size, message);

    if (success && appState.session.partitionId == id){
        appState.session = Session();
    }
    return {success, message};
}
