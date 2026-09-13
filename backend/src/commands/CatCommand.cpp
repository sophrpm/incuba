#include "commands/CatCommand.hpp"

#include <algorithm>
#include <cctype>

#include "managers/FileManager.hpp"

using namespace std;


//ejecuta cat
ValidationResult CatCommand::execute(const ParsedCommand& command, const AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name.size() < 5 || param.name.substr(0, 4) != "file"){
            return {false, "CAT: parametro no reconocido -" + param.name + "."};
        }

        string numberText = param.name.substr(4);

        int number = 0;
        //despues de file solo deben venir numeros
        for (char character : numberText){
            if (character < '0' || character > '9' || number > (2147483647 - (character - '0')) / 10){
                return {false, "CAT: parametro no reconocido -" + param.name + "."};
            }
            number = number * 10 + character - '0';
        }
        if (number <= 0 || param.name != "file" + to_string(number)){
            return {false, "CAT: use -fileN con un entero positivo sin ceros iniciales."};
        }
        if (command.countParam(param.name) > 1 || !param.hasValue || param.value.empty()){
            return {false, "CAT: parametro repetido o sin valor -" + param.name + "."};
        }
        if (param.value[0] != '/'){
            return {false, "CAT: la ruta debe ser absoluta."};
        }
    }

    //debe venir al menos un archivo
    if (command.params.empty()){
        return {false, "CAT: debe indicar al menos un parametro -fileN."};
    }

    vector<string> files = getFiles(command);

    if (files.empty()){
        return {false, "CAT: no se encontraron archivos validos para leer."};
    }

    FileManager fileManager;
    string finalContent;

    //lee los archivos en orden
    for (size_t filePos = 0; filePos < files.size(); filePos++){
        string content;
        string message;

        if (!fileManager.readFile(files[filePos], appState, content, message)){
            return {false, "CAT: no se pudo leer " + files[filePos] + ". " + message};
        }

        finalContent += content;

        //separa archivos cuando hay mas de uno
        if (filePos + 1 < files.size()){
            finalContent += "\n";
        }
    }

    return {true, finalContent};
}


//obtiene los archivos enviados como -file1, -file2...
vector<string> CatCommand::getFiles(const ParsedCommand& command) const {
    vector<pair<int, string>> orderedFiles;

    for (const ParsedParam& param : command.params){
        if (param.name.size() < 5 || param.name.substr(0, 4) != "file"){
            continue;
        }

        if (!param.hasValue || param.value.empty()){
            continue;
        }

        string numberText = param.name.substr(4);
        int number = 0;

        //convierte el numero del parametro
        for (char character : numberText){
            if (character < '0' || character > '9'){
                number = -1;
                break;
            }

            number = number * 10 + (character - '0');
        }

        if (number <= 0){
            continue;
        }

        orderedFiles.push_back({number, param.value});
    }

    //ordena por file1, file2, file3...
    sort(orderedFiles.begin(), orderedFiles.end(), [](const pair<int, string>& firstFile, const pair<int, string>& secondFile){
        return firstFile.first < secondFile.first;
    });

    vector<string> files;

    for (const pair<int, string>& file : orderedFiles){
        files.push_back(file.second);
    }

    return files;
}
