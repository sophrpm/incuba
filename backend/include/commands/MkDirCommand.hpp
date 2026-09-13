#ifndef MKDIR_COMMAND_HPP
#define MKDIR_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mkdir
class MkDirCommand {
public:

    //ejecuta mkdir
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
