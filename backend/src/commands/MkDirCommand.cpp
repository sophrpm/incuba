#include "commands/MkDirCommand.hpp"

#include "managers/FileManager.hpp"

using namespace std;


//ejecuta mkdir
ValidationResult MkDirCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "path" && param.name != "p"){
            return {false, "MKDIR: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("path") > 1){
        return {false, "MKDIR: el parametro -path esta repetido."};
    }

    if (command.countParam("p") > 1){
        return {false, "MKDIR: el parametro -p esta repetido."};
    }

    //path es obligatorio
    if (!command.hasParam("path")){
        return {false, "MKDIR: falta el parametro obligatorio -path."};
    }

    //path necesita valor
    if (!command.paramHasValue("path")){
        return {false, "MKDIR: el parametro -path necesita un valor."};
    }

    //-p es bandera y no debe llevar valor
    if (command.hasParam("p") && command.paramHasValue("p")){
        return {false, "MKDIR: el parametro -p no recibe valor."};
    }

    string path = command.getParam("path");

    if (path.empty()){
        return {false, "MKDIR: el valor de -path no puede estar vacio."};
    }

    //verifica que sea ruta absoluta dentro de ext2
    if (path[0] != '/'){
        return {false, "MKDIR: -path debe ser una ruta absoluta."};
    }

    bool recursive = command.hasParam("p");

    FileManager fileManager;
    string message;

    //crea la carpeta
    bool success = fileManager.createDirectory(path, recursive, appState, message);

    return {success, message};
}
