#include "commands/LoginCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta login
ValidationResult LoginCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "user" && param.name != "pass" && param.name != "id"){
            return {false, "LOGIN: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("user") > 1){
        return {false, "LOGIN: el parametro -user esta repetido."};
    }

    if (command.countParam("pass") > 1){
        return {false, "LOGIN: el parametro -pass esta repetido."};
    }

    if (command.countParam("id") > 1){
        return {false, "LOGIN: el parametro -id esta repetido."};
    }

    //todos son obligatorios
    if (!command.hasParam("user")){
        return {false, "LOGIN: falta el parametro obligatorio -user."};
    }

    if (!command.hasParam("pass")){
        return {false, "LOGIN: falta el parametro obligatorio -pass."};
    }

    if (!command.hasParam("id")){
        return {false, "LOGIN: falta el parametro obligatorio -id."};
    }

    //verifica que tengan valor
    if (!command.paramHasValue("user")){
        return {false, "LOGIN: el parametro -user necesita un valor."};
    }

    if (!command.paramHasValue("pass")){
        return {false, "LOGIN: el parametro -pass necesita un valor."};
    }

    if (!command.paramHasValue("id")){
        return {false, "LOGIN: el parametro -id necesita un valor."};
    }

    string user = command.getParam("user");
    string password = command.getParam("pass");
    string id = command.getParam("id");

    if (user.empty()){
        return {false, "LOGIN: el valor de -user no puede estar vacio."};
    }

    if (password.empty()){
        return {false, "LOGIN: el valor de -pass no puede estar vacio."};
    }

    if (id.empty()){
        return {false, "LOGIN: el valor de -id no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //intenta iniciar sesion
    bool success = userManager.login(user, password, id, appState, message);

    return {success, message};
}
