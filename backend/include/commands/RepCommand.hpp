#ifndef REP_COMMAND_HPP
#define REP_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando rep
class RepCommand {
public:

    //ejecuta rep
    ValidationResult execute(const ParsedCommand& command, const AppState& appState) const;
};


#endif
