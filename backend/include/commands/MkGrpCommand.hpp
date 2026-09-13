#ifndef MKGRP_COMMAND_HPP
#define MKGRP_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mkgrp
class MkGrpCommand {
public:

    //ejecuta mkgrp
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
