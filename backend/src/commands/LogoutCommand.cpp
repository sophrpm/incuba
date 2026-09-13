#include "commands/LogoutCommand.hpp"

#include "managers/UserManager.hpp"

using namespace std;


//ejecuta logout
ValidationResult LogoutCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //logout no recibe parametros
    if (!command.params.empty()){
        return {false, "LOGOUT: este comando no recibe parametros."};
    }

    UserManager userManager;
    string message;

    //cierra la sesion actual
    bool success = userManager.logout(appState, message);

    return {success, message};
}
