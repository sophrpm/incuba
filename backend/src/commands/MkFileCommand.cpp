#include "commands/MkFileCommand.hpp"

#include <fstream>

#include "managers/FileManager.hpp"

using namespace std;


//ejecuta mkfile
ValidationResult MkFileCommand::execute(const ParsedCommand& command, AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "path" && param.name != "r" && param.name != "size" && param.name != "cont"){
            return {false, "MKFILE: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("path") > 1){
        return {false, "MKFILE: el parametro -path esta repetido."};
    }

    if (command.countParam("r") > 1){
        return {false, "MKFILE: el parametro -r esta repetido."};
    }

    if (command.countParam("size") > 1){
        return {false, "MKFILE: el parametro -size esta repetido."};
    }

    if (command.countParam("cont") > 1){
        return {false, "MKFILE: el parametro -cont esta repetido."};
    }

    //path es obligatorio
    if (!command.hasParam("path")){
        return {false, "MKFILE: falta el parametro obligatorio -path."};
    }

    if (!command.paramHasValue("path")){
        return {false, "MKFILE: el parametro -path necesita un valor."};
    }

    //-r es bandera
    if (command.hasParam("r") && command.paramHasValue("r")){
        return {false, "MKFILE: el parametro -r no recibe valor."};
    }

    //size necesita valor si viene
    if (command.hasParam("size") && !command.paramHasValue("size")){
        return {false, "MKFILE: el parametro -size necesita un valor."};
    }

    //cont necesita valor si viene
    if (command.hasParam("cont") && !command.paramHasValue("cont")){
        return {false, "MKFILE: el parametro -cont necesita un valor."};
    }

    string path = command.getParam("path");

    if (path.empty()){
        return {false, "MKFILE: el valor de -path no puede estar vacio."};
    }

    //la ruta dentro de ext2 debe ser absoluta
    if (path[0] != '/'){
        return {false, "MKFILE: -path debe ser una ruta absoluta."};
    }

    if (!appState.session.active){
        return {false, "MKFILE: debe iniciar sesion."};
    }

    bool recursive = command.hasParam("r");
    string content = "";

    //si viene -cont tiene prioridad
    if (command.hasParam("cont")){
        string externalPath = command.getParam("cont");

        if (externalPath.empty()){
            return {false, "MKFILE: el valor de -cont no puede estar vacio."};
        }

        if (!readExternalFile(externalPath, content)){
            return {false, "MKFILE: no se pudo leer el archivo indicado en -cont."};
        }
    }

    //si no viene cont usa size
    else if (command.hasParam("size")){
        string sizeText = command.getParam("size");

        if (sizeText.empty()){
            return {false, "MKFILE: el valor de -size no puede estar vacio."};
        }

        long long sizeValue = 0;

        //convierte size manualmente
        for (char character : sizeText){
            if (character < '0' || character > '9'){
                return {false, "MKFILE: el parametro -size debe ser un numero entero positivo."};
            }

            int digit = character - '0';

            if (sizeValue > (2147483647LL - digit) / 10){
                return {false, "MKFILE: el valor de -size es demasiado grande."};
            }

            sizeValue = sizeValue * 10 + digit;
        }

        if (sizeValue < 0){
            return {false, "MKFILE: el parametro -size no puede ser negativo."};
        }

        if (sizeValue > 4380 * 64){
            return {false, "MKFILE: el tamaño supera la capacidad de los apuntadores (280320 bytes)."};
        }
        content = generateContent(static_cast<int>(sizeValue));
    }

    FileManager fileManager;
    string message;

    //primero intenta crear sin sobreescribir
    bool success = fileManager.createFile(path, content, recursive, false, appState, message);

    return {success, message};
}


//genera contenido usando 0123456789
string MkFileCommand::generateContent(int size) const {
    string content;
    string pattern = "0123456789";

    if (size <= 0){
        return content;
    }

    content.reserve(size);

    //repite patron hasta completar tamaño
    for (int position = 0; position < size; position++){
        content += pattern[position % 10];
    }

    return content;
}


//lee contenido desde un archivo del sistema
bool MkFileCommand::readExternalFile(const string& path, string& content) const {
    ifstream file(path, ios::in | ios::binary | ios::ate);

    if (!file.is_open()){
        return false;
    }

    streamoff size = file.tellg();
    if (size < 0 || size > 4380 * 64){
        return false;
    }
    file.seekg(0);
    content = "";

    char character;

    //lee archivo completo
    while (file.get(character)){
        content += character;
    }

    return file.eof() && !file.bad();
}
