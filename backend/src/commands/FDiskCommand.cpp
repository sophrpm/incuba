#include "commands/FDiskCommand.hpp"

#include <climits>

#include "managers/DiskManager.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//ejecuta fdisk
ValidationResult FDiskCommand::execute(const ParsedCommand& command) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "size" && param.name != "unit" && param.name != "path" && param.name != "type" && param.name != "fit" && param.name != "delete" && param.name != "name" && param.name != "add"){
            return {false, "FDISK: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("size") > 1){
        return {false, "FDISK: el parametro -size esta repetido."};
    }

    if (command.countParam("unit") > 1){
        return {false, "FDISK: el parametro -unit esta repetido."};
    }

    if (command.countParam("path") > 1){
        return {false, "FDISK: el parametro -path esta repetido."};
    }

    if (command.countParam("type") > 1){
        return {false, "FDISK: el parametro -type esta repetido."};
    }

    if (command.countParam("fit") > 1){
        return {false, "FDISK: el parametro -fit esta repetido."};
    }

    if (command.countParam("delete") > 1){
        return {false, "FDISK: el parametro -delete esta repetido."};
    }

    if (command.countParam("name") > 1){
        return {false, "FDISK: el parametro -name esta repetido."};
    }

    if (command.countParam("add") > 1){
        return {false, "FDISK: el parametro -add esta repetido."};
    }

    //path y name siempre son obligatorios
    if (!command.hasParam("path")){
        return {false, "FDISK: falta el parametro obligatorio -path."};
    }

    if (!command.hasParam("name")){
        return {false, "FDISK: falta el parametro obligatorio -name."};
    }

    if (!command.paramHasValue("path")){
        return {false, "FDISK: el parametro -path necesita un valor."};
    }

    if (!command.paramHasValue("name")){
        return {false, "FDISK: el parametro -name necesita un valor."};
    }

    string path = command.getParam("path");
    string name = command.getParam("name");

    if (path.empty()){
        return {false, "FDISK: el valor de -path no puede estar vacio."};
    }

    if (name.empty()){
        return {false, "FDISK: el valor de -name no puede estar vacio."};
    }

    if (name.size() > 16){
        return {false, "FDISK: el nombre de la particion no puede superar 16 caracteres."};
    }

    bool deleteOperation = isDeleteOperation(command);
    bool addOperation = isAddOperation(command);

    //no permite mezclar operaciones
    if (deleteOperation && addOperation){
        return {false, "FDISK: no se puede usar -delete y -add al mismo tiempo."};
    }

    DiskManager diskManager;
    string message;

    //eliminar
    if (deleteOperation){

        if (!command.paramHasValue("delete")){
            return {false, "FDISK: el parametro -delete necesita un valor."};
        }

        string deleteType = StringUtils::toLower(command.getParam("delete"));

        if (deleteType != "full"){
            return {false, "FDISK: -delete solo acepta full."};
        }

        //no debe mezclar parametros de creacion
        if (command.hasParam("size") || command.hasParam("type") || command.hasParam("fit") || command.hasParam("add")){
            return {false, "FDISK: -delete no puede combinarse con parametros de creacion o modificacion."};
        }

        if (command.hasParam("unit") && (!command.paramHasValue("unit") || (StringUtils::toUpper(command.getParam("unit")) != "B" && StringUtils::toUpper(command.getParam("unit")) != "K" && StringUtils::toUpper(command.getParam("unit")) != "M"))){
            return {false, "FDISK: -unit solo acepta B, K o M."};
        }
        bool success = diskManager.deletePartition(path, name, message);

        return {success, message};
    }

    //modificar
    if (addOperation){

        if (!command.paramHasValue("add")){
            return {false, "FDISK: el parametro -add necesita un valor."};
        }

        if (command.hasParam("size") || command.hasParam("type") || command.hasParam("fit") || command.hasParam("delete")){
            return {false, "FDISK: -add no puede combinarse con parametros de creacion o eliminacion."};
        }

        string addText = command.getParam("add");

        if (addText.empty()){
            return {false, "FDISK: el valor de -add no puede estar vacio."};
        }

        int sign = 1;
        size_t startPosition = 0;

        //acepta numero negativo
        if (addText[0] == '-'){
            sign = -1;
            startPosition = 1;
        } else if (addText[0] == '+'){
            startPosition = 1;
        }

        if (startPosition >= addText.size()){
            return {false, "FDISK: el valor de -add no es valido."};
        }

        long long addValue = 0;

        //convierte add manualmente
        for (size_t position = startPosition; position < addText.size(); position++){
            char character = addText[position];

            if (character < '0' || character > '9'){
                return {false, "FDISK: el parametro -add debe ser un numero entero."};
            }

            int digit = character - '0';

            if (addValue > (LLONG_MAX - digit) / 10){
                return {false, "FDISK: el valor de -add es demasiado grande."};
            }

            addValue = addValue * 10 + digit;
        }

        addValue *= sign;

        if (addValue == 0){
            return {false, "FDISK: el parametro -add no puede ser cero."};
        }

        string unit = "K";

        if (command.hasParam("unit")){
            if (!command.paramHasValue("unit")){
                return {false, "FDISK: el parametro -unit necesita un valor."};
            }

            unit = StringUtils::toUpper(command.getParam("unit"));
        }

        if (unit != "B" && unit != "K" && unit != "M"){
            return {false, "FDISK: -unit solo acepta B, K o M."};
        }

        long long multiplier = unit == "M" ? 1024 * 1024 : (unit == "K" ? 1024 : 1);
        if (addValue > INT_MAX / multiplier || addValue < INT_MIN / multiplier){
            return {false, "FDISK: el tamaño de modificacion es demasiado grande."};
        }
        long long addBytes;

        if (unit == "B"){
            addBytes = addValue;
        } else if (unit == "K"){
            addBytes = addValue * 1024;
        } else {
            addBytes = addValue * 1024 * 1024;
        }

        if (addBytes > INT_MAX || addBytes < INT_MIN){
            return {false, "FDISK: el tamaño de modificacion es demasiado grande."};
        }

        bool success = diskManager.resizePartition(path, name, static_cast<int>(addBytes), message);

        return {success, message};
    }

    //crear
    if (!command.hasParam("size")){
        return {false, "FDISK: falta el parametro obligatorio -size para crear una particion."};
    }

    if (!command.paramHasValue("size")){
        return {false, "FDISK: el parametro -size necesita un valor."};
    }

    string sizeText = command.getParam("size");

    if (sizeText.empty()){
        return {false, "FDISK: el valor de -size no puede estar vacio."};
    }

    long long sizeValue = 0;

    //convierte size manualmente
    for (char character : sizeText){
        if (character < '0' || character > '9'){
            return {false, "FDISK: el parametro -size debe ser un numero entero positivo."};
        }

        int digit = character - '0';

        if (sizeValue > (LLONG_MAX - digit) / 10){
            return {false, "FDISK: el valor de -size es demasiado grande."};
        }

        sizeValue = sizeValue * 10 + digit;
    }

    if (sizeValue <= 0){
        return {false, "FDISK: el parametro -size debe ser mayor que cero."};
    }

    //valores por defecto
    string unit = "K";
    string typeText = "P";
    string fitText = "WF";

    if (command.hasParam("unit")){
        if (!command.paramHasValue("unit")){
            return {false, "FDISK: el parametro -unit necesita un valor."};
        }

        unit = StringUtils::toUpper(command.getParam("unit"));
    }

    if (command.hasParam("type")){
        if (!command.paramHasValue("type")){
            return {false, "FDISK: el parametro -type necesita un valor."};
        }

        typeText = StringUtils::toUpper(command.getParam("type"));
    }

    if (command.hasParam("fit")){
        if (!command.paramHasValue("fit")){
            return {false, "FDISK: el parametro -fit necesita un valor."};
        }

        fitText = StringUtils::toUpper(command.getParam("fit"));
    }

    //valida unidad
    if (unit != "B" && unit != "K" && unit != "M"){
        return {false, "FDISK: -unit solo acepta B, K o M."};
    }

    char type = getType(typeText);

    if (type == '\0'){
        return {false, "FDISK: -type solo acepta P, E o L."};
    }

    char fit = getFit(fitText);

    if (fit == '\0'){
        return {false, "FDISK: -fit solo acepta BF, FF o WF."};
    }

    if (sizeValue > INT_MAX){
        return {false, "FDISK: el valor de -size es demasiado grande."};
    }

    long long sizeBytes = getSizeInBytes(static_cast<int>(sizeValue), unit);

    if (sizeBytes <= 0 || sizeBytes > INT_MAX){
        return {false, "FDISK: el tamaño final de la particion es demasiado grande."};
    }

    bool success = diskManager.createPartition(path, static_cast<int>(sizeBytes), type, fit, name, message);

    return {success, message};
}


//convierte tamaño y unidad a bytes
long long FDiskCommand::getSizeInBytes(int size, const string& unit) const {
    string upperUnit = StringUtils::toUpper(unit);

    if (upperUnit == "B"){
        return size;
    }

    if (upperUnit == "K"){
        return static_cast<long long>(size) * 1024;
    }

    if (upperUnit == "M"){
        return static_cast<long long>(size) * 1024 * 1024;
    }

    return -1;
}


//obtiene tipo de particion
char FDiskCommand::getType(const string& type) const {
    string upperType = StringUtils::toUpper(type);

    if (upperType == "P"){
        return 'P';
    }

    if (upperType == "E"){
        return 'E';
    }

    if (upperType == "L"){
        return 'L';
    }

    return '\0';
}


//obtiene fit de particion
char FDiskCommand::getFit(const string& fit) const {
    string upperFit = StringUtils::toUpper(fit);

    if (upperFit == "BF"){
        return 'B';
    }

    if (upperFit == "FF"){
        return 'F';
    }

    if (upperFit == "WF"){
        return 'W';
    }

    return '\0';
}


//verifica si se quiere eliminar
bool FDiskCommand::isDeleteOperation(const ParsedCommand& command) const {
    return command.hasParam("delete");
}


//verifica si se quiere modificar tamaño
bool FDiskCommand::isAddOperation(const ParsedCommand& command) const {
    return command.hasParam("add");
}
