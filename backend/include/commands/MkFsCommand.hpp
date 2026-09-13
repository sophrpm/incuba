#ifndef MKFS_COMMAND_HPP
#define MKFS_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mkfs
class MkFsCommand {
public:

    //ejecuta mkfs
    ValidationResult execute(const ParsedCommand& command, AppState& appState) const;
};


#endif
