#ifndef RMDISK_COMMAND_HPP
#define RMDISK_COMMAND_HPP

#include "analysis/ParsedCommand.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando rmdisk
class RmDiskCommand {
public:

    //ejecuta rmdisk
    ValidationResult execute(const ParsedCommand& command) const;
};


#endif
