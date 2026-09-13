#ifndef CHGRP_COMMAND_HPP
#define CHGRP_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando chgrp
class ChGrpCommand {
public:

    //ejecuta chgrp
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
