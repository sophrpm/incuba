#include <crow.h>
#include <crow/middlewares/cors.h>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <map>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include "managers/DiskManager.hpp"
#include "managers/MountManager.hpp"
#include "managers/FileSystemManager.hpp"
#include "managers/UserManager.hpp"
#include "utils/BinaryUtils.hpp"
#include "managers/FileManager.hpp"
#include "utils/StringUtils.hpp"
#include "utils/PathUtils.hpp"

#include "analysis/CommandParser.hpp"
#include "analysis/CommandToken.hpp"
#include "analysis/ParsedCommand.hpp"

#include "commands/CatCommand.hpp"
#include "commands/ChGrpCommand.hpp"
#include "commands/FDiskCommand.hpp"
#include "commands/LoginCommand.hpp"
#include "commands/LogoutCommand.hpp"
#include "commands/MkDirCommand.hpp"
#include "commands/MkDiskCommand.hpp"
#include "commands/MkFileCommand.hpp"
#include "commands/MkFsCommand.hpp"
#include "commands/MkGrpCommand.hpp"
#include "commands/MkUsrCommand.hpp"
#include "commands/MountCommand.hpp"
#include "commands/MountedCommand.hpp"
#include "commands/RepCommand.hpp"
#include "commands/RmDiskCommand.hpp"
#include "commands/RmGrpCommand.hpp"
#include "commands/RmUsrCommand.hpp"

#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//guarda montajes y sesion mientras corre el servidor
AppState appState;
mutex appMutex;
vector<string> reportPaths;
string diskRoot;
string reportRoot;
string inputRoot;
struct BrowserState {
    Session session;
    PendingConfirmation pending;
    vector<unsigned int> reports;
};
map<string, BrowserState> browsers;
string activeBrowser;

//tokens aleatorios, el navegador nunca elige uid ni gid
string newToken(){
    random_device random;
    ostringstream token;
    for (int i = 0; i < 8; i++) token << hex << setw(8) << setfill('0') << random();
    return token.str();
}

bool useBrowser(const crow::request& request){
    string token = request.get_header_value("X-MIA-Session");
    auto browser = browsers.find(token);
    if (browser == browsers.end()) return false;
    activeBrowser = token;
    appState.session = browser->second.session;
    appState.pendingConfirmation = browser->second.pending;
    return true;
}

void saveBrowser(){
    browsers[activeBrowser].session = appState.session;
    browsers[activeBrowser].pending = appState.pendingConfirmation;
    appState.session = Session();
    appState.pendingConfirmation = PendingConfirmation();
}

crow::response jsonMessage(int status, bool success, const string& message){
    crow::json::wvalue body;
    body["success"] = success;
    body["message"] = message;
    body["messages"] = vector<string>{message};
    return crow::response(status, body);
}

//las rutas externas quedan dentro de directorios configurados
bool safeHostPath(string& path, const string& root){
    if (path.empty() || path.find('\0') != string::npos) return false;
    filesystem::path given(path);
    if (!given.is_absolute()) given = filesystem::path(root) / given;
    string resolved = PathUtils::canonicalPath(given.string());
    if (resolved.size() <= root.size() || resolved.compare(0, root.size() + 1, root + "/") != 0) return false;
    path = resolved;
    return true;
}

void invalidatePartition(const string& id){
    for (auto& browser : browsers){
        if (browser.second.session.partitionId == id){
            browser.second.session = Session();
            browser.second.pending = PendingConfirmation();
        }
    }
    if (appState.session.partitionId == id){
        appState.session = Session();
        appState.pendingConfirmation = PendingConfirmation();
    }
}


//ejecuta el comando que corresponde
ValidationResult executeRaw(const ParsedCommand& command){

    if (command.name == "mkdisk"){
        MkDiskCommand mkDiskCommand;
        return mkDiskCommand.execute(command);
    }

    if (command.name == "rmdisk"){
        RmDiskCommand rmDiskCommand;
        ValidationResult result = rmDiskCommand.execute(command);
        if (result.success){
            string path = PathUtils::canonicalPath(command.getParam("path"));
            for (size_t i = 0; i < appState.mountedPartitions.size();){
                if (appState.mountedPartitions[i].path == path){
                    invalidatePartition(appState.mountedPartitions[i].id);
                    appState.mountedPartitions.erase(appState.mountedPartitions.begin() + i);
                } else {
                    i++;
                }
            }
        }
        return result;
    }

    if (command.name == "fdisk"){
        FDiskCommand fDiskCommand;
        return fDiskCommand.execute(command);
    }

    if (command.name == "mount"){
        MountCommand mountCommand;
        return mountCommand.execute(command, appState);
    }

    if (command.name == "mounted"){
        MountedCommand mountedCommand;
        return mountedCommand.execute(command, appState);
    }

    if (command.name == "mkfs"){
        MkFsCommand mkFsCommand;
        return mkFsCommand.execute(command, appState);
    }

    if (command.name == "login"){
        LoginCommand loginCommand;
        return loginCommand.execute(command, appState);
    }

    if (command.name == "logout"){
        LogoutCommand logoutCommand;
        return logoutCommand.execute(command, appState);
    }

    if (command.name == "mkgrp"){
        MkGrpCommand mkGrpCommand;
        return mkGrpCommand.execute(command, appState);
    }

    if (command.name == "rmgrp"){
        RmGrpCommand rmGrpCommand;
        return rmGrpCommand.execute(command, appState);
    }

    if (command.name == "mkusr"){
        MkUsrCommand mkUsrCommand;
        return mkUsrCommand.execute(command, appState);
    }

    if (command.name == "rmusr"){
        RmUsrCommand rmUsrCommand;
        return rmUsrCommand.execute(command, appState);
    }

    if (command.name == "chgrp"){
        ChGrpCommand chGrpCommand;
        return chGrpCommand.execute(command, appState);
    }

    if (command.name == "mkdir"){
        MkDirCommand mkDirCommand;
        return mkDirCommand.execute(command, appState);
    }

    if (command.name == "mkfile"){
        MkFileCommand mkFileCommand;
        return mkFileCommand.execute(command, appState);
    }

    if (command.name == "cat"){
        CatCommand catCommand;
        return catCommand.execute(command, appState);
    }

    if (command.name == "rep"){
        RepCommand repCommand;
        return repCommand.execute(command, appState);
    }

    string message;
    if (command.name == "unmount"){
        MountManager manager;
        bool success = manager.unmountPartition(command.getParam("id"), appState, message);
        if (success) invalidatePartition(command.getParam("id"));
        return {success, message};
    }
    if (command.name == "loss" || command.name == "journaling"){
        MountManager mounts;
        MountedPartition partition;
        if (!mounts.getMountedPartition(appState, command.getParam("id"), partition)) return {false, "Error: particion no montada."};
        FileSystemManager fs;
        if (command.name == "loss"){
            bool success = fs.simulateLoss(partition.path, partition.start, message);
            if (success) invalidatePartition(partition.id);
            return {success, message};
        }
        Journal journal;
        if (!fs.readJournal(partition.path, partition.start, journal)) return {false, "Error: esta particion no tiene journal EXT3."};
        message = "Journal: " + to_string(journal.j_count) + " operaciones.\n";
        for (int i = 0; i < journal.j_count; i++){
            const Information& entry = journal.j_content[i];
            message += BinaryUtils::fixedCharToString(entry.i_operation, 10) + " | " + BinaryUtils::fixedCharToString(entry.i_path, 32) + " | " + BinaryUtils::fixedCharToString(entry.i_content, 64) + " | " + to_string(static_cast<long long>(entry.i_date)) + "\n";
        }
        return {true, message};
    }
    FileManager files;
    if (command.name == "remove") return {files.remove(command.getParam("path"), appState, message), message};
    if (command.name == "rename") return {files.rename(command.getParam("path"), command.getParam("name"), appState, message), message};
    if (command.name == "copy") return {files.copy(command.getParam("path"), command.getParam("destino"), appState, message), message};
    if (command.name == "move") return {files.move(command.getParam("path"), command.getParam("destino"), appState, message), message};
    if (command.name == "find") return {files.find(command.getParam("path"), command.getParam("name"), appState, message), message};

    return {false, "Error: comando no reconocido " + command.name + "."};
}


//valida rutas y mantiene cada cambio completo antes de responder
ValidationResult executeCommand(ParsedCommand command, bool confirmed = false){
    if (command.name == "fdisk"){
        string operation;
        for (const ParsedParam& param : command.params){
            if (param.name == "size" || param.name == "add" || param.name == "delete") { operation = param.name; break; }
        }
        for (size_t i = 0; i < command.params.size();){
            string key = command.params[i].name;
            bool otherOperation = (key == "size" || key == "add" || key == "delete") && key != operation;
            bool creationOnly = operation != "size" && (key == "type" || key == "fit");
            if (otherOperation || creationOnly) command.params.erase(command.params.begin() + i);
            else i++;
        }
    }
    string required;
    if (command.name == "unmount" || command.name == "loss" || command.name == "journaling") required = " id ";
    if (command.name == "remove") required = " path ";
    if (command.name == "rename" || command.name == "find") required = " path name ";
    if (command.name == "copy" || command.name == "move") required = " path destino ";
    if (command.name == "find" && command.getParam("name").size() > 256) return {false, "Error: patron demasiado largo."};
    if (!required.empty()){
        for (const ParsedParam& param : command.params){
            if (required.find(" " + param.name + " ") == string::npos || !param.hasValue || param.value.empty() || command.countParam(param.name) != 1) return {false, "Error: parametro invalido -" + param.name + "."};
        }
        istringstream keys(required);
        string key;
        while (keys >> key){
            if (!command.hasParam(key)) return {false, "Error: falta -" + key + "."};
        }
    }
    bool hostDisk = command.name == "mkdisk" || command.name == "rmdisk" || command.name == "fdisk" || command.name == "mount";
    for (ParsedParam& param : command.params){
        if (param.value.find('\0') != string::npos) return {false, "Error: valor invalido."};
        if (param.name == "path" && (hostDisk || command.name == "rep" || command.name == "exec")){
            string root = hostDisk ? diskRoot : (command.name == "rep" ? reportRoot : inputRoot);
            if (!safeHostPath(param.value, root)) return {false, "Error: ruta externa fuera del directorio autorizado."};
            if (command.name == "rep"){
                string dotPath = param.value + ".dot";
                if (!safeHostPath(dotPath, reportRoot)) return {false, "Error: ruta DOT fuera del directorio autorizado."};
            }
        } else if (param.name == "cont"){
            if (!safeHostPath(param.value, inputRoot)) return {false, "Error: -cont debe estar en MIA_INPUT_DIR."};
        } else if (param.name == "path" || param.name == "destino" || param.name == "path_file_ls" || param.name.compare(0, 4, "file") == 0){
            if (param.value.empty() || param.value[0] != '/') return {false, "Error: la ruta virtual debe ser absoluta."};
            if (param.value.size() > 4096) return {false, "Error: ruta virtual demasiado larga."};
            filesystem::path route(param.value);
            for (const auto& part : route){
                if (part == "." || part == "..") return {false, "Error: use rutas virtuales sin . ni .."};
            }
            param.value = route.lexically_normal().string();
            while (param.value.size() > 1 && param.value.back() == '/') param.value.pop_back();
        }
    }
    if (command.name == "rep" && (command.getParam("name") == "file" || command.getParam("name") == "ls")){
        if (!appState.session.active || appState.session.partitionId != command.getParam("id")) return {false, "Error: REP FILE/LS requiere sesion en esa particion."};
    }
    if (command.name == "fdisk" && command.hasParam("delete") && !confirmed){
        FDiskCommand validator;
        ValidationResult valid = validator.execute(command, true);
        if (!valid.success) return valid;
        //pregunta HTTP; no se usa cin
        DiskManager disks;
        MBR mbr;
        EBR ebr;
        int position;
        string path = command.getParam("path");
        string name = command.getParam("name");
        string type = StringUtils::toLower(command.getParam("delete"));
        if (type != "fast" && type != "full") return {false, "Error: -delete acepta fast o full."};
        if (!disks.readMBR(path, mbr) || (disks.findPartition(mbr, name) < 0 && !disks.findLogicalPartition(path, name, ebr, position))) return {false, "Error: particion inexistente."};
        for (const MountedPartition& mounted : appState.mountedPartitions){
            if (mounted.path == path && StringUtils::equalsIgnoreCase(mounted.name, name)) return {false, "Error: debe desmontar antes de eliminar."};
        }
        appState.pendingConfirmation.active = true;
        appState.pendingConfirmation.operation = "fdisk";
        appState.pendingConfirmation.path = path;
        appState.pendingConfirmation.name = name;
        appState.pendingConfirmation.deleteType = type;
        return {true, "¿Confirma eliminar la particion " + name + "? [y/n]"};
    }
    const string modifications = " mkdir mkfile mkgrp rmgrp mkusr rmusr chgrp remove rename copy move loss mkfs fdisk mount unmount ";
    bool modifying = modifications.find(" " + command.name + " ") != string::npos;
    const string fsChanges = " mkdir mkfile mkgrp rmgrp mkusr rmusr chgrp remove rename copy move ";
    bool journaled = fsChanges.find(" " + command.name + " ") != string::npos;
    MountedPartition partition;
    MountManager mounts;
    bool mounted = mounts.getMountedPartition(appState, appState.session.partitionId, partition);
    FileSystemManager fs;
    SuperBlock sb;
    bool ext3 = false;
    if (journaled && mounted){
        if (!fs.readSuperBlock(partition.path, partition.start, sb) || sb.s_magic != 0xEF53) return {false, "Error: sistema de archivos invalido."};
        char used;
        if (!fs.readInodeBitmap(partition.path, sb, 0, used) || used != '1') return {false, "Error: particion con LOSS; debe formatear."};
        ext3 = sb.s_filesystem_type == 3;
        if (ext3){
            Journal journal;
            if (!fs.readJournal(partition.path, partition.start, journal) || journal.j_count == JOURNAL_CAPACITY) return {false, "Error: journal lleno o invalido; no se realizaron cambios."};
        }
    }
    string disk;
    if (modifying){
        if (hostDisk) disk = command.getParam("path");
        else if (command.hasParam("id")){
            MountedPartition selected;
            if (mounts.getMountedPartition(appState, command.getParam("id"), selected)) disk = selected.path;
        } else if (mounted) disk = partition.path;
    }
    string backup;
    error_code error;
    if (!disk.empty()){
        backup = diskRoot + "/.transactions/" + newToken();
        filesystem::copy_file(disk, backup, filesystem::copy_options::overwrite_existing, error);
        if (error) return {false, "Error: no se pudo reservar respaldo para el cambio."};
    }
    AppState previous = appState;
    ValidationResult result = executeRaw(command);
    if (result.success && ext3){
        string route = command.hasParam("path") ? command.getParam("path") : "/users.txt";
        string content;
        if (command.name == "mkfile"){
            FileManager files;
            string ignored;
            files.readFile(route, appState, content, ignored);
        } else if (command.hasParam("name")) content = command.getParam("name");
        else if (command.hasParam("destino")) content = command.getParam("destino");
        else if (command.hasParam("user")) content = command.getParam("user");
        //nunca persiste el password de mkusr en la bitacora
        if (!fs.appendJournal(partition.path, partition.start, command.name, route, content, result.message)) result.success = false;
    }
    if (!result.success && !backup.empty()){
        filesystem::copy_file(backup, disk, filesystem::copy_options::overwrite_existing, error);
        PendingConfirmation pending = appState.pendingConfirmation;
        appState = previous;
        if (pending.active) appState.pendingConfirmation = pending;
        if (error) result.message += " Error al restaurar respaldo: " + backup;
    }
    if (!backup.empty() && !error) filesystem::remove(backup, error);
    if (result.success && command.name == "mkfs") invalidatePartition(command.getParam("id"));
    return result;
}

int scriptDepth = 0;

//procesa lineas en orden y conserva una continuacion si hay una pregunta
crow::json::wvalue processInput(string inputText){
    vector<string> messages;
    vector<crow::json::wvalue> reports;
    int successfulCommands = 0;
    int lexicalErrors = 0;
    int syntaxErrors = 0;
    bool allSuccess = true;

    if (appState.pendingConfirmation.active){
        string answer = StringUtils::toLower(StringUtils::trim(inputText));
        PendingConfirmation pending = appState.pendingConfirmation;
        successfulCommands = pending.successfulCommands;
        lexicalErrors = pending.lexicalErrors;
        syntaxErrors = pending.syntaxErrors;
        allSuccess = pending.allSuccess;
        if (answer != "y" && answer != "yes" && answer != "n" && answer != "no"){
            inputText = "";
            messages.push_back("Responda y/yes o n/no para confirmar la operacion [y/n]:");
        } else {
            appState.pendingConfirmation = PendingConfirmation();
            if (answer == "y" || answer == "yes"){
                FileManager fileManager;
                string message;
                bool success;
                if (pending.operation == "fdisk"){
                    ParsedCommand deletion;
                    deletion.name = "fdisk";
                    deletion.params = {{"path", pending.path, true}, {"name", pending.name, true}, {"delete", pending.deleteType, true}};
                    ValidationResult result = executeCommand(deletion, true);
                    success = result.success;
                    message = result.message;
                } else {
                    MountedPartition mounted;
                    MountManager mounts;
                    FileSystemManager fs;
                    SuperBlock sb;
                    success = mounts.getMountedPartition(appState, appState.session.partitionId, mounted) && fs.readSuperBlock(mounted.path, mounted.start, sb);
                    bool ext3 = success && sb.s_filesystem_type == 3;
                    Journal journal;
                    if (ext3 && (!fs.readJournal(mounted.path, mounted.start, journal) || journal.j_count == JOURNAL_CAPACITY)){
                        success = false;
                        message = "Error: journal lleno; sobrescritura cancelada.";
                    }
                    string backup = diskRoot + "/.transactions/" + newToken();
                    error_code error;
                    if (success){
                        filesystem::copy_file(mounted.path, backup, filesystem::copy_options::overwrite_existing, error);
                        success = !error;
                    }
                    if (success){
                        success = fileManager.createFile(pending.path, pending.content, pending.recursive, true, appState, message);
                        if (success && ext3) success = fs.appendJournal(mounted.path, mounted.start, "mkfile", pending.path, pending.content, message);
                        if (!success) filesystem::copy_file(backup, mounted.path, filesystem::copy_options::overwrite_existing, error);
                    }
                    if (!error) filesystem::remove(backup, error);
                }
                messages.push_back(message);
                if (success){
                    successfulCommands++;
                } else {
                    allSuccess = false;
                }
            } else {
                messages.push_back("Operacion cancelada.");
            }
            inputText = pending.remainingInput;
        }
    }

    size_t position = 0;
    while (position < inputText.size()){
        size_t end = inputText.find('\n', position);
        size_t next = end == string::npos ? inputText.size() : end + 1;
        string line = inputText.substr(position, (end == string::npos ? inputText.size() : end) - position);
        if (!line.empty() && line.back() == '\r'){
            line.pop_back();
        }
        position = next;
        string trimmed = StringUtils::trim(line);
        if (trimmed.empty() || trimmed[0] == '#'){
            messages.push_back(line);
            continue;
        }
        CommandToken tokenizer;
        TokenizeResult tokens = tokenizer.tokenize(line);
        if (!tokens.errors.empty()){
            lexicalErrors += static_cast<int>(tokens.errors.size());
            messages.insert(messages.end(), tokens.errors.begin(), tokens.errors.end());
            allSuccess = false;
            continue;
        }
        CommandParser parser;
        ParseResult parsed = parser.parse(tokens.tokens);
        if (!parsed.errors.empty()){
            syntaxErrors += static_cast<int>(parsed.errors.size());
            messages.insert(messages.end(), parsed.errors.begin(), parsed.errors.end());
            allSuccess = false;
            continue;
        }
        for (const ParsedCommand& command : parsed.commands){
            if (command.isComment){
                messages.push_back(command.commentText);
                continue;
            }
            if (command.name == "exec"){
                string path = command.getParam("path");
                if (command.params.size() != 1 || !command.paramHasValue("path") || !safeHostPath(path, inputRoot) || scriptDepth >= 8){
                    messages.push_back("Error: EXEC requiere path dentro de MIA_INPUT_DIR y profundidad maxima 8.");
                    allSuccess = false;
                    continue;
                }
                ifstream script(path, ios::binary | ios::ate);
                if (!script || script.tellg() > 1024 * 1024){
                    messages.push_back("Error: script inexistente o mayor de 1 MiB.");
                    allSuccess = false;
                    continue;
                }
                script.seekg(0);
                string content((istreambuf_iterator<char>(script)), istreambuf_iterator<char>());
                scriptDepth++;
                auto nested = processInput(content);
                scriptDepth--;
                auto result = crow::json::load(nested.dump());
                for (const auto& msg : result["messages"]) messages.push_back(msg.s());
                successfulCommands += result["stats"]["successfulCommands"].i();
                lexicalErrors += result["stats"]["lexicalErrors"].i();
                syntaxErrors += result["stats"]["syntaxErrors"].i();
                allSuccess = allSuccess && result["success"].b();
                if (appState.pendingConfirmation.active){
                    appState.pendingConfirmation.remainingInput += "\n" + inputText.substr(position);
                    appState.pendingConfirmation.successfulCommands = successfulCommands;
                    appState.pendingConfirmation.lexicalErrors = lexicalErrors;
                    appState.pendingConfirmation.syntaxErrors = syntaxErrors;
                    appState.pendingConfirmation.allSuccess = allSuccess;
                    break;
                }
                continue;
            }
            ValidationResult result = executeCommand(command);
            messages.push_back(result.message);
            if (appState.pendingConfirmation.active){
                appState.pendingConfirmation.remainingInput = inputText.substr(position);
                for (const ParsedCommand& remaining : parsed.commands){
                    if (remaining.isComment){
                        appState.pendingConfirmation.remainingInput = remaining.commentText + "\n" + appState.pendingConfirmation.remainingInput;
                    }
                }
                appState.pendingConfirmation.successfulCommands = successfulCommands;
                appState.pendingConfirmation.lexicalErrors = lexicalErrors;
                appState.pendingConfirmation.syntaxErrors = syntaxErrors;
                appState.pendingConfirmation.allSuccess = allSuccess;
                break;
            }
            if (result.success){
                successfulCommands++;
                if (command.name == "rep"){
                    string path = command.getParam("path");
                    safeHostPath(path, reportRoot);
                    reportPaths.push_back(path);
                    browsers[activeBrowser].reports.push_back(reportPaths.size() - 1);
                    crow::json::wvalue report;
                    report["name"] = PathUtils::getFileName(path);
                    report["url"] = "/api/reports/" + to_string(reportPaths.size() - 1);
                    reports.push_back(move(report));
                }
            } else {
                allSuccess = false;
            }
        }
        if (appState.pendingConfirmation.active){
            break;
        }
    }

    crow::json::wvalue response;
    response["success"] = allSuccess;
    response["messages"] = messages;
    response["stats"]["successfulCommands"] = successfulCommands;
    response["stats"]["lexicalErrors"] = lexicalErrors;
    response["stats"]["syntaxErrors"] = syntaxErrors;
    response["pendingConfirmation"] = appState.pendingConfirmation.active;
    response["reports"] = move(reports);
    return response;
}


//inicia el servidor
int main(){
    diskRoot = PathUtils::canonicalPath(getenv("MIA_DISK_DIR") ? getenv("MIA_DISK_DIR") : "data/disks");
    reportRoot = PathUtils::canonicalPath(getenv("MIA_REPORT_DIR") ? getenv("MIA_REPORT_DIR") : "data/reports");
    inputRoot = PathUtils::canonicalPath(getenv("MIA_INPUT_DIR") ? getenv("MIA_INPUT_DIR") : "data/inputs");
    error_code directoryError;
    filesystem::create_directories(diskRoot + "/.transactions", directoryError);
    filesystem::create_directories(reportRoot, directoryError);
    filesystem::create_directories(inputRoot, directoryError);
    if (directoryError){ cerr << "No se pudieron crear directorios de datos." << endl; return 1; }
    DiskManager disks;
    filesystem::recursive_directory_iterator diskFiles(diskRoot, filesystem::directory_options::skip_permission_denied, directoryError);
    for (const auto& file : diskFiles){
        string path = file.path().string();
        if (!file.is_regular_file(directoryError) || !PathUtils::hasExtension(path, "mia") || !safeHostPath(path, diskRoot)) continue;
        MBR mbr;
        if (!disks.readMBR(path, mbr)) continue;
        for (const Partition& partition : mbr.mbr_partitions){
            string id = BinaryUtils::fixedCharToString(partition.part_id, 4);
            if (partition.part_status != '1' || partition.part_type != 'P' || id.size() != 4) continue;
            MountedPartition mounted;
            mounted.id = id;
            mounted.path = path;
            mounted.name = BinaryUtils::fixedCharToString(partition.part_name, 16);
            mounted.start = partition.part_start;
            mounted.size = partition.part_s;
            mounted.correlative = partition.part_correlative;
            mounted.diskLetter = id.back();
            appState.mountedPartitions.push_back(mounted);
        }
    }
    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global().headers("Content-Type", "X-MIA-Session").methods("GET"_method, "POST"_method, "OPTIONS"_method).origin(getenv("MIA_CORS_ORIGIN") ? getenv("MIA_CORS_ORIGIN") : "http://localhost:5173");

    CROW_ROUTE(app, "/api/health")([](){
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "Servidor MIA activo.";
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/api/analyze").methods(crow::HTTPMethod::POST)([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (request.body.size() > 1024 * 1024) return jsonMessage(413, false, "Error: script mayor de 1 MiB.");
        crow::json::rvalue body = crow::json::load(request.body);
        if (!body || !body.has("input") || body["input"].t() != crow::json::type::String){
            crow::json::wvalue response;
            response["success"] = false;
            response["messages"] = vector<string>{"Error: se necesita un JSON con input de tipo texto."};
            response["stats"]["successfulCommands"] = 0;
            response["stats"]["lexicalErrors"] = 0;
            response["stats"]["syntaxErrors"] = 0;
            return crow::response(400, response);
        }
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida; consulte /api/session.");
        crow::json::wvalue result = processInput(body["input"].s());
        bool success = crow::json::load(result.dump())["success"].b();
        saveBrowser();
        return crow::response(success ? 200 : 400, result);
    });

    //solo permite consultar reportes generados en esta ejecucion
    CROW_ROUTE(app, "/api/reports/<uint>")([](const crow::request& request, unsigned int index){
        lock_guard<mutex> lock(appMutex);
        string token = request.get_header_value("X-MIA-Session");
        auto browser = browsers.find(token);
        if (browser == browsers.end() || std::find(browser->second.reports.begin(), browser->second.reports.end(), index) == browser->second.reports.end()) return crow::response(401);
        if (index >= reportPaths.size()){
            return crow::response(404);
        }
        ifstream file(reportPaths[index], ios::binary);
        if (!file){
            return crow::response(404);
        }
        string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
        string extension = StringUtils::toLower(PathUtils::getExtension(reportPaths[index]));
        string type = "text/plain; charset=utf-8";
        if (extension == "svg") type = "image/svg+xml";
        if (extension == "png") type = "image/png";
        if (extension == "jpg" || extension == "jpeg") type = "image/jpeg";
        if (extension == "pdf") type = "application/pdf";
        crow::response response(200, content);
        response.set_header("Content-Type", type);
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_header("Content-Security-Policy", "sandbox");
        return response;
    });

    //cada navegador recibe su propio token y estado de autenticacion
    CROW_ROUTE(app, "/api/session")([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        string token = request.get_header_value("X-MIA-Session");
        if (browsers.find(token) == browsers.end()){
            token = newToken();
            browsers[token] = BrowserState();
        }
        const Session& session = browsers[token].session;
        crow::json::wvalue body;
        body["success"] = true;
        body["token"] = token;
        body["active"] = session.active;
        body["user"] = session.user;
        body["partitionId"] = session.partitionId;
        body["uid"] = session.uid;
        body["gid"] = session.gid;
        return crow::response(200, body);
    });

    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::POST)([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        auto body = crow::json::load(request.body);
        if (!body || !body.has("user") || !body.has("password") || !body.has("id") || body["user"].t() != crow::json::type::String || body["password"].t() != crow::json::type::String || body["id"].t() != crow::json::type::String) return jsonMessage(400, false, "Error: requiere user, password e id de tipo texto.");
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        UserManager users;
        string message;
        bool success = users.login(body["user"].s(), body["password"].s(), body["id"].s(), appState, message);
        saveBrowser();
        return jsonMessage(success ? 200 : 401, success, message);
    });
    CROW_ROUTE(app, "/api/logout").methods(crow::HTTPMethod::POST)([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        UserManager users;
        string message;
        bool success = users.logout(appState, message);
        appState.pendingConfirmation = PendingConfirmation();
        saveBrowser();
        return jsonMessage(success ? 200 : 400, success, message);
    });

    CROW_ROUTE(app, "/api/disks")([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        vector<crow::json::wvalue> disks;
        DiskManager manager;
        FileSystemManager fs;
        error_code error;
        filesystem::recursive_directory_iterator iterator(diskRoot, filesystem::directory_options::skip_permission_denied, error);
        for (const auto& file : iterator){
            if (!file.is_regular_file(error) || !PathUtils::hasExtension(file.path().string(), "mia")) continue;
            string path = file.path().string();
            if (!safeHostPath(path, diskRoot)) continue;
            MBR mbr;
            if (!manager.readMBR(path, mbr)) continue;
            crow::json::wvalue disk;
            disk["id"] = filesystem::relative(path, diskRoot, error).string();
            disk["name"] = file.path().filename().string();
            disk["size"] = mbr.mbr_tamano;
            disk["fit"] = string(1, mbr.dsk_fit);
            disk["date"] = static_cast<int64_t>(mbr.mbr_fecha_creacion);
            vector<crow::json::wvalue> partitions;
            for (const Partition& partition : mbr.mbr_partitions){
                if (partition.part_s <= 0) continue;
                crow::json::wvalue item;
                item["name"] = BinaryUtils::fixedCharToString(partition.part_name, 16);
                item["size"] = partition.part_s;
                item["fit"] = string(1, partition.part_fit);
                item["type"] = string(1, partition.part_type);
                string mountId;
                for (const MountedPartition& mounted : appState.mountedPartitions){
                    if (mounted.path == path && mounted.start == partition.part_start) mountId = mounted.id;
                }
                item["id"] = mountId;
                item["mounted"] = !mountId.empty();
                SuperBlock sb;
                int format = partition.part_type == 'P' && fs.readSuperBlock(path, partition.part_start, sb) && sb.s_magic == 0xEF53 ? sb.s_filesystem_type : 0;
                item["filesystem"] = format;
                partitions.push_back(move(item));
                if (partition.part_type != 'E') continue;
                int position = partition.part_start;
                int end = position + partition.part_s;
                while (position >= partition.part_start && position + static_cast<int>(sizeof(EBR)) <= end){
                    EBR ebr;
                    if (!BinaryUtils::readStruct(path, position, ebr)) break;
                    if (ebr.part_s > 0){
                        crow::json::wvalue logical;
                        logical["name"] = BinaryUtils::fixedCharToString(ebr.part_name, 16);
                        logical["size"] = ebr.part_s;
                        logical["fit"] = string(1, ebr.part_fit);
                        logical["type"] = "L";
                        logical["id"] = "";
                        logical["mounted"] = false;
                        logical["filesystem"] = 0;
                        partitions.push_back(move(logical));
                    }
                    if (ebr.part_next <= position || ebr.part_next >= end) break;
                    position = ebr.part_next;
                }
            }
            disk["partitions"] = move(partitions);
            disks.push_back(move(disk));
        }
        crow::json::wvalue body;
        body["success"] = !error;
        body["disks"] = move(disks);
        saveBrowser();
        return crow::response(error ? 500 : 200, body);
    });

    CROW_ROUTE(app, "/api/files")([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        if (!appState.session.active){ saveBrowser(); return jsonMessage(401, false, "Debe iniciar sesion."); }
        string path = request.url_params.get("path") ? request.url_params.get("path") : "/";
        FileManager files;
        vector<Content> entries;
        string message;
        if (!files.getDirectoryContent(path, appState, entries, message)){ saveBrowser(); return jsonMessage(403, false, message); }
        MountedPartition partition;
        MountManager mounts;
        mounts.getMountedPartition(appState, appState.session.partitionId, partition);
        FileSystemManager fs;
        SuperBlock sb;
        fs.readSuperBlock(partition.path, partition.start, sb);
        vector<crow::json::wvalue> items;
        for (const Content& entry : entries){
            string name = BinaryUtils::fixedCharToString(entry.b_name, 12);
            if (name == "." || name == "..") continue;
            Inode inode;
            if (!fs.readInode(partition.path, sb, entry.b_inodo, inode)) continue;
            crow::json::wvalue item;
            item["name"] = name;
            item["path"] = path == "/" ? "/" + name : path + "/" + name;
            item["type"] = inode.i_type == '0' ? "directory" : "file";
            item["size"] = inode.i_s;
            item["uid"] = inode.i_uid;
            item["gid"] = inode.i_gid;
            item["permissions"] = BinaryUtils::fixedCharToString(inode.i_perm, 3);
            items.push_back(move(item));
        }
        crow::json::wvalue body;
        body["success"] = true;
        body["path"] = path;
        body["entries"] = move(items);
        saveBrowser();
        return crow::response(200, body);
    });

    CROW_ROUTE(app, "/api/files/content")([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        if (!appState.session.active){ saveBrowser(); return jsonMessage(401, false, "Debe iniciar sesion."); }
        const char* path = request.url_params.get("path");
        FileManager files;
        string content;
        string message;
        bool success = path && files.readFile(path, appState, content, message);
        saveBrowser();
        if (!success) return jsonMessage(403, false, message.empty() ? "Error: falta path." : message);
        if (!StringUtils::isText(content)) return jsonMessage(415, false, "Error: archivo binario o texto no UTF-8 compatible con el visor.");
        crow::json::wvalue body;
        body["success"] = true;
        body["content"] = content;
        return crow::response(200, body);
    });

    CROW_ROUTE(app, "/api/journaling/<string>")([](const crow::request& request, string id){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        MountedPartition partition;
        MountManager mounts;
        bool exists = mounts.getMountedPartition(appState, id, partition);
        saveBrowser();
        if (!exists) return jsonMessage(404, false, "Error: particion no montada.");
        FileSystemManager fs;
        Journal journal;
        if (!fs.readJournal(partition.path, partition.start, journal)) return jsonMessage(400, false, "La particion EXT2 no tiene journaling o su formato es invalido.");
        vector<crow::json::wvalue> entries;
        for (int i = 0; i < journal.j_count; i++){
            const Information& entry = journal.j_content[i];
            crow::json::wvalue item;
            item["operation"] = BinaryUtils::fixedCharToString(entry.i_operation, 10);
            item["path"] = BinaryUtils::fixedCharToString(entry.i_path, 32);
            string content = BinaryUtils::fixedCharToString(entry.i_content, 64);
            item["content"] = StringUtils::isText(content) ? content : "[contenido binario]";
            item["date"] = static_cast<int64_t>(entry.i_date);
            entries.push_back(move(item));
        }
        crow::json::wvalue body;
        body["success"] = true;
        body["capacity"] = JOURNAL_CAPACITY;
        body["entries"] = move(entries);
        return crow::response(200, body);
    });

    CROW_ROUTE(app, "/api/reports")([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
        if (!useBrowser(request)) return jsonMessage(401, false, "Error: sesion de navegador invalida.");
        vector<crow::json::wvalue> reports;
        for (unsigned int index : browsers[activeBrowser].reports){
            crow::json::wvalue item;
            item["name"] = PathUtils::getFileName(reportPaths[index]);
            item["url"] = "/api/reports/" + to_string(index);
            reports.push_back(move(item));
        }
        crow::json::wvalue body;
        body["success"] = true;
        body["reports"] = move(reports);
        saveBrowser();
        return crow::response(200, body);
    });

    const char* portText = getenv("MIA_PORT");
    int port = portText ? StringUtils::toPositiveInt(portText) : 2611;
    if (port < 1 || port > 65535){
        cerr << "MIA_PORT debe estar entre 1 y 65535." << endl;
        return 1;
    }
    cout << "Servidor MIA ejecutandose en http://localhost:" << port << endl;
    app.bindaddr(getenv("MIA_HOST") ? getenv("MIA_HOST") : "127.0.0.1").port(port).multithreaded().run();
    return 0;
}
