#include <crow.h>
#include <crow/middlewares/cors.h>

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <filesystem>
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


//ejecuta el comando que corresponde
ValidationResult executeCommand(const ParsedCommand& command){

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
                    if (appState.session.partitionId == appState.mountedPartitions[i].id){
                        appState.session = Session();
                    }
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

    return {false, "Error: comando no reconocido " + command.name + "."};
}


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
            messages.push_back("Responda y/yes o n/no. ¿Desea sobrescribirlo? [y/n]:");
        } else {
            appState.pendingConfirmation = PendingConfirmation();
            if (answer == "y" || answer == "yes"){
                FileManager fileManager;
                string message;
                bool success = fileManager.createFile(pending.path, pending.content, pending.recursive, true, appState, message);
                messages.push_back(message);
                if (success){
                    successfulCommands++;
                } else {
                    allSuccess = false;
                }
            } else {
                messages.push_back("Sobrescritura cancelada.");
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
                    reportPaths.push_back(path);
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
    crow::App<crow::CORSHandler> app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global().headers("Content-Type").methods("GET"_method, "POST"_method, "OPTIONS"_method).origin("*");

    CROW_ROUTE(app, "/api/health")([](){
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "Servidor MIA activo.";
        return crow::response(200, response);
    });

    CROW_ROUTE(app, "/api/analyze").methods(crow::HTTPMethod::POST)([](const crow::request& request){
        lock_guard<mutex> lock(appMutex);
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
        return crow::response(200, processInput(body["input"].s()));
    });

    //solo permite consultar reportes generados en esta ejecucion
    CROW_ROUTE(app, "/api/reports/<uint>")([](unsigned int index){
        lock_guard<mutex> lock(appMutex);
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

    const char* portText = getenv("MIA_PORT");
    int port = portText ? StringUtils::toPositiveInt(portText) : 2611;
    if (port < 1 || port > 65535){
        cerr << "MIA_PORT debe estar entre 1 y 65535." << endl;
        return 1;
    }
    cout << "Servidor MIA ejecutandose en http://localhost:" << port << endl;
    app.bindaddr("127.0.0.1").port(port).multithreaded().run();
    return 0;
}
