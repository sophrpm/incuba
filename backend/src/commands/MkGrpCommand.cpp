#include "commands/MkGrpCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta mkgrp
ValidationResult MkGrpCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "name"){
            return {false, "MKGRP: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametro repetido
    if (command.countParam("name") > 1){
        return {false, "MKGRP: el parametro -name esta repetido."};
    }

    //name es obligatorio
    if (!command.hasParam("name")){
        return {false, "MKGRP: falta el parametro obligatorio -name."};
    }

    //name necesita valor
    if (!command.paramHasValue("name")){
        return {false, "MKGRP: el parametro -name necesita un valor."};
    }

    string name = command.getParam("name");

    if (name.empty()){
        return {false, "MKGRP: el valor de -name no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //crea el grupo
    bool success = userManager.createGroup(name, appState, message);

    return {success, message};
}
