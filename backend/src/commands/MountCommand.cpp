#include "commands/MountCommand.hpp"

#include "managers/MountManager.hpp"

using namespace std;


//ejecuta mount
ValidationResult MountCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "path" && param.name != "name"){
            return {false, "MOUNT: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("path") > 1){
        return {false, "MOUNT: el parametro -path esta repetido."};
    }

    if (command.countParam("name") > 1){
        return {false, "MOUNT: el parametro -name esta repetido."};
    }

    //path es obligatorio
    if (!command.hasParam("path")){
        return {false, "MOUNT: falta el parametro obligatorio -path."};
    }

    //name es obligatorio
    if (!command.hasParam("name")){
        return {false, "MOUNT: falta el parametro obligatorio -name."};
    }

    //verifica que tengan valor
    if (!command.paramHasValue("path")){
        return {false, "MOUNT: el parametro -path necesita un valor."};
    }

    if (!command.paramHasValue("name")){
        return {false, "MOUNT: el parametro -name necesita un valor."};
    }

    string path = command.getParam("path");
    string name = command.getParam("name");

    if (path.empty()){
        return {false, "MOUNT: el valor de -path no puede estar vacio."};
    }

    if (name.empty()){
        return {false, "MOUNT: el valor de -name no puede estar vacio."};
    }

    MountManager mountManager;
    string message;

    //monta la particion
    bool success = mountManager.mountPartition(path, name, appState, message);

    return {success, message};
}
