#include "commands/RepCommand.hpp"

#include "managers/ReportManager.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//ejecuta rep
ValidationResult RepCommand::execute(const ParsedCommand& command, const AppState& appState) const {

    //verifica parametros permitidos
    for (const ParsedParam& param : command.params){
        if (param.name != "name" && param.name != "path" && param.name != "id" && param.name != "path_file_ls"){
            return {false, "REP: parametro no reconocido -" + param.name + "."};
        }
    }

    //verifica parametros repetidos
    if (command.countParam("name") > 1){
        return {false, "REP: el parametro -name esta repetido."};
    }

    if (command.countParam("path") > 1){
        return {false, "REP: el parametro -path esta repetido."};
    }

    if (command.countParam("id") > 1){
        return {false, "REP: el parametro -id esta repetido."};
    }

    if (command.countParam("path_file_ls") > 1){
        return {false, "REP: el parametro -path_file_ls esta repetido."};
    }

    //name, path e id son obligatorios
    if (!command.hasParam("name")){
        return {false, "REP: falta el parametro obligatorio -name."};
    }

    if (!command.hasParam("path")){
        return {false, "REP: falta el parametro obligatorio -path."};
    }

    if (!command.hasParam("id")){
        return {false, "REP: falta el parametro obligatorio -id."};
    }

    //verifica que tengan valor
    if (!command.paramHasValue("name")){
        return {false, "REP: el parametro -name necesita un valor."};
    }

    if (!command.paramHasValue("path")){
        return {false, "REP: el parametro -path necesita un valor."};
    }

    if (!command.paramHasValue("id")){
        return {false, "REP: el parametro -id necesita un valor."};
    }

    if (command.hasParam("path_file_ls") && !command.paramHasValue("path_file_ls")){
        return {false, "REP: el parametro -path_file_ls necesita un valor."};
    }

    string name = StringUtils::toLower(command.getParam("name"));
    string path = command.getParam("path");
    string id = command.getParam("id");
    string pathFileLs = "";

    if (name.empty()){
        return {false, "REP: el valor de -name no puede estar vacio."};
    }

    if (path.empty()){
        return {false, "REP: el valor de -path no puede estar vacio."};
    }

    if (id.empty()){
        return {false, "REP: el valor de -id no puede estar vacio."};
    }

    if (command.hasParam("path_file_ls")){
        pathFileLs = command.getParam("path_file_ls");

        if (pathFileLs.empty()){
            return {false, "REP: el valor de -path_file_ls no puede estar vacio."};
        }
    }

    //verifica tipo de reporte
    if (name != "mbr" && name != "disk" && name != "inode" && name != "block" && name != "bm_inode" && name != "bm_block" && name != "tree" && name != "sb" && name != "file" && name != "ls"){
        return {false, "REP: tipo de reporte no reconocido."};
    }

    //file y ls necesitan ruta interna
    if ((name == "file" || name == "ls") && pathFileLs.empty()){
        return {false, "REP: el reporte " + name + " necesita -path_file_ls."};
    }

    //los demas no necesitan path_file_ls
    if (name != "file" && name != "ls" && command.hasParam("path_file_ls")){
        return {false, "REP: -path_file_ls solo se utiliza con los reportes file y ls."};
    }

    ReportManager reportManager;
    string message;

    //genera el reporte
    bool success = reportManager.generateReport(name, path, id, pathFileLs, appState, message);

    return {success, message};
}
