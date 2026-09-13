#include "commands/RmUsrCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta rmusr
ValidationResult RmUsrCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "user"){
            return {false, "RMUSR: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametro repetido
    if (command.countParam("user") > 1){
        return {false, "RMUSR: el parametro -user esta repetido."};
    }

    //user es obligatorio
    if (!command.hasParam("user")){
        return {false, "RMUSR: falta el parametro obligatorio -user."};
    }

    //user necesita valor
    if (!command.paramHasValue("user")){
        return {false, "RMUSR: el parametro -user necesita un valor."};
    }

    string user = command.getParam("user");

    if (user.empty()){
        return {false, "RMUSR: el valor de -user no puede estar vacio."};
    }

    UserManager userManager;
    string message;

    //elimina el usuario
    bool success = userManager.removeUser(user, appState, message);

    return {success, message};
}
