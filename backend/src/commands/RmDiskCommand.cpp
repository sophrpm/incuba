#include "commands/RmDiskCommand.hpp"

#include "managers/DiskManager.hpp"

using namespace std;


//ejecuta rmdisk
ValidationResult RmDiskCommand::execute(const ParsedCommand& command) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "path"){
            return {false, "RMDISK: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametro repetido
    if (command.countParam("path") > 1){
        return {false, "RMDISK: el parametro -path esta repetido."};
    }

    //path es obligatorio
    if (!command.hasParam("path")){
        return {false, "RMDISK: falta el parametro obligatorio -path."};
    }

    //path necesita valor
    if (!command.paramHasValue("path")){
        return {false, "RMDISK: el parametro -path necesita un valor."};
    }

    string path = command.getParam("path");

    if (path.empty()){
        return {false, "RMDISK: el valor de -path no puede estar vacio."};
    }

    DiskManager diskManager;
    string message;

    //elimina el disco real
    bool success = diskManager.removeDisk(path, message);

    return {success, message};
}
