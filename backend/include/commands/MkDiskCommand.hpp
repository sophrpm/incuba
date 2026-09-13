#ifndef MKDISK_COMMAND_HPP
#define MKDISK_COMMAND_HPP

#include <string>

#include "analysis/ParsedCommand.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando mkdisk
class MkDiskCommand {
public:

    //ejecuta mkdisk
    ValidationResult execute(const ParsedCommand& command) const;

private:

    //convierte tamaño y unidad a bytes
    long long getSizeInBytes(int size, const string& unit) const;

    //convierte fit al valor que usa el disco
    char getFit(const string& fit) const;
};


#endif
