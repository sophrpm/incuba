#include "commands/MkDiskCommand.hpp"

#include <climits>

#include "managers/DiskManager.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//ejecuta mkdisk
ValidationResult MkDiskCommand::execute(const ParsedCommand& command) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "size" && param.name != "fit" && param.name != "unit" && param.name != "path"){
            return {false, "MKDISK: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("size") > 1){
        return {false, "MKDISK: el parametro -size esta repetido."};
    }

    if (command.countParam("fit") > 1){
        return {false, "MKDISK: el parametro -fit esta repetido."};
    }

    if (command.countParam("unit") > 1){
        return {false, "MKDISK: el parametro -unit esta repetido."};
    }

    if (command.countParam("path") > 1){
        return {false, "MKDISK: el parametro -path esta repetido."};
    }

    //size es obligatorio
    if (!command.hasParam("size")){
        return {false, "MKDISK: falta el parametro obligatorio -size."};
    }

    //path es obligatorio
    if (!command.hasParam("path")){
        return {false, "MKDISK: falta el parametro obligatorio -path."};
    }

    //verifica que size tenga valor
    if (!command.paramHasValue("size")){
        return {false, "MKDISK: el parametro -size necesita un valor."};
    }

    //verifica que path tenga valor
    if (!command.paramHasValue("path")){
        return {false, "MKDISK: el parametro -path necesita un valor."};
    }

    //si vienen deben tener valor
    if (command.hasParam("fit") && !command.paramHasValue("fit")){
        return {false, "MKDISK: el parametro -fit necesita un valor."};
    }

    if (command.hasParam("unit") && !command.paramHasValue("unit")){
        return {false, "MKDISK: el parametro -unit necesita un valor."};
    }

    string sizeText = command.getParam("size");
    string path = command.getParam("path");

    if (sizeText.empty()){
        return {false, "MKDISK: el valor de -size no puede estar vacio."};
    }

    if (path.empty()){
        return {false, "MKDISK: el valor de -path no puede estar vacio."};
    }

    //convierte size sin usar try/catch
    long long sizeValue = 0;

    for (char character : sizeText){
        if (character < '0' || character > '9'){
            return {false, "MKDISK: el parametro -size debe ser un numero entero positivo."};
        }

        int digit = character - '0';

        if (sizeValue > (LLONG_MAX - digit) / 10){
            return {false, "MKDISK: el valor de -size es demasiado grande."};
        }

        sizeValue = sizeValue * 10 + digit;
    }

    if (sizeValue <= 0){
        return {false, "MKDISK: el parametro -size debe ser mayor que cero."};
    }

    //valores por defecto
    string unit = "M";
    string fitText = "FF";

    if (command.hasParam("unit")){
        unit = StringUtils::toUpper(command.getParam("unit"));
    }

    if (command.hasParam("fit")){
        fitText = StringUtils::toUpper(command.getParam("fit"));
    }

    //valida unidad
    if (unit != "K" && unit != "M"){
        return {false, "MKDISK: -unit solo acepta K o M."};
    }

    //valida fit
    char fit = getFit(fitText);

    if (fit == '\0'){
        return {false, "MKDISK: -fit solo acepta BF, FF o WF."};
    }

    //el manager usa int para tamaños
    if (sizeValue > INT_MAX){
        return {false, "MKDISK: el valor de -size es demasiado grande."};
    }

    long long sizeBytes = getSizeInBytes(static_cast<int>(sizeValue), unit);

    if (sizeBytes <= 0 || sizeBytes > INT_MAX){
        return {false, "MKDISK: el tamaño final del disco es demasiado grande."};
    }

    DiskManager diskManager;
    string message;

    //crea el disco real
    bool success = diskManager.createDisk(path, static_cast<int>(sizeBytes), fit, message);

    return {success, message};
}


//convierte tamaño y unidad a bytes
long long MkDiskCommand::getSizeInBytes(int size, const string& unit) const {
    string upperUnit = StringUtils::toUpper(unit);

    if (upperUnit == "K"){
        return static_cast<long long>(size) * 1024;
    }

    if (upperUnit == "M"){
        return static_cast<long long>(size) * 1024 * 1024;
    }

    return -1;
}


//convierte fit al valor que usa el disco
char MkDiskCommand::getFit(const string& fit) const {
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
