#ifndef MKUSR_COMMAND_HPP
#define MKUSR_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mkusr
class MkUsrCommand {
public:

    //ejecuta mkusr
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
