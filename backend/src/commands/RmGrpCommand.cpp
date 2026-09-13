#include "commands/RmGrpCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta rmgrp
ValidationResult RmGrpCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "name"){
            return {false, "RMGRP: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametro repetido
    if (command.countParam("name") > 1){
        return {false, "RMGRP: el parametro -name esta repetido."};
    }

    //name es obligatorio
    if (!command.hasParam("name")){
        return {false, "RMGRP: falta el parametro obligatorio -name."};
    }

    //name necesita valor
    if (!command.paramHasValue("name")){
        return {false, "RMGRP: el parametro -name necesita un valor."};
    }

    string name = command.getParam("name");

    if (name.empty()){
        return {false, "RMGRP: el valor de -name no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //elimina el grupo
    bool success = userManager.removeGroup(name, appState, message);

    return {success, message};
}
