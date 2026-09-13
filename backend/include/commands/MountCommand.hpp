#ifndef MOUNT_COMMAND_HPP
#define MOUNT_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mount
class MountCommand {
public:

    //ejecuta mount
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
