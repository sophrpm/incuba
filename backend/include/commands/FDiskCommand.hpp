#ifndef FDISK_COMMAND_HPP
#define FDISK_COMMAND_HPP

#include <string>

#include "analysis/ParsedCommand.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando fdisk
class FDiskCommand {
public:

    //ejecuta fdisk
    ValidationResult execute(const ParsedCommand& command) const;


private:

    //convierte tamaño y unidad a bytes
    long long getSizeInBytes(int size, const string& unit) const;

    //obtiene tipo de particion
    char getType(const string& type) const;

    //obtiene fit de particion
    char getFit(const string& fit) const;

    //verifica si se quiere eliminar
    bool isDeleteOperation(const ParsedCommand& command) const;

    //verifica si se quiere modificar tamaño
    bool isAddOperation(const ParsedCommand& command) const;
};


#endif
