#include "commands/ChGrpCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta chgrp
ValidationResult ChGrpCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "user" && param.name != "grp"){
            return {false, "CHGRP: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("user") > 1){
        return {false, "CHGRP: el parametro -user esta repetido."};
    }

    if (command.countParam("grp") > 1){
        return {false, "CHGRP: el parametro -grp esta repetido."};
    }

    //todos son obligatorios
    if (!command.hasParam("user")){
        return {false, "CHGRP: falta el parametro obligatorio -user."};
    }

    if (!command.hasParam("grp")){
        return {false, "CHGRP: falta el parametro obligatorio -grp."};
    }

    //verifica que tengan valor
    if (!command.paramHasValue("user")){
        return {false, "CHGRP: el parametro -user necesita un valor."};
    }

    if (!command.paramHasValue("grp")){
        return {false, "CHGRP: el parametro -grp necesita un valor."};
    }

    string user = command.getParam("user");
    string group = command.getParam("grp");

    if (user.empty()){
        return {false, "CHGRP: el valor de -user no puede estar vacio."};
    }

    if (group.empty()){
        return {false, "CHGRP: el valor de -grp no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //cambia el grupo del usuario
    bool success = userManager.changeUserGroup(user, group, appState, message);

    return {success, message};
}
