#ifndef MOUNTED_COMMAND_HPP
#define MOUNTED_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mounted
class MountedCommand {
public:

    //ejecuta mounted
    ValidationResult execute(const ParsedCommand& command, const AppState& appState) const;
};


#endif
