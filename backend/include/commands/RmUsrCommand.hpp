#ifndef RMUSR_COMMAND_HPP
#define RMUSR_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando rmusr
class RmUsrCommand {
public:

    //ejecuta rmusr
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
