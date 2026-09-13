#include "commands/MkUsrCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta mkusr
ValidationResult MkUsrCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "user" && param.name != "pass" && param.name != "grp"){
            return {false, "MKUSR: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("user") > 1){
        return {false, "MKUSR: el parametro -user esta repetido."};
    }

    if (command.countParam("pass") > 1){
        return {false, "MKUSR: el parametro -pass esta repetido."};
    }

    if (command.countParam("grp") > 1){
        return {false, "MKUSR: el parametro -grp esta repetido."};
    }

    //todos son obligatorios
    if (!command.hasParam("user")){
        return {false, "MKUSR: falta el parametro obligatorio -user."};
    }

    if (!command.hasParam("pass")){
        return {false, "MKUSR: falta el parametro obligatorio -pass."};
    }

    if (!command.hasParam("grp")){
        return {false, "MKUSR: falta el parametro obligatorio -grp."};
    }

    //verifica que tengan valor
    if (!command.paramHasValue("user")){
        return {false, "MKUSR: el parametro -user necesita un valor."};
    }

    if (!command.paramHasValue("pass")){
        return {false, "MKUSR: el parametro -pass necesita un valor."};
    }

    if (!command.paramHasValue("grp")){
        return {false, "MKUSR: el parametro -grp necesita un valor."};
    }

    string user = command.getParam("user");
    string password = command.getParam("pass");
    string group = command.getParam("grp");

    if (user.empty()){
        return {false, "MKUSR: el valor de -user no puede estar vacio."};
    }

    if (password.empty()){
        return {false, "MKUSR: el valor de -pass no puede estar vacio."};
    }

    if (group.empty()){
        return {false, "MKUSR: el valor de -grp no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //crea el usuario
    bool success = userManager.createUser(user, password, group, appState, message);

    return {success, message};
}
