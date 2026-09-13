#include "managers/UserManager.hpp"

#include "managers/FileManager.hpp"
#include "managers/MountManager.hpp"
#include "state/AppState.hpp"
#include "state/Session.hpp"
#include "utils/StringUtils.hpp"

using namespace std;


//inicia sesion con un usuario
bool UserManager::login(const string& user, const string& password, const string& id, AppState& appState, string& message) const {
    if (hasActiveSession(appState)){
        message = "Error: ya existe una sesion activa.";
        return false;
    }

    MountManager mountManager;
    MountedPartition mountedPartition;

    //verifica que la particion este montada
    if (!mountManager.getMountedPartition(appState, id, mountedPartition)){
        message = "Error: no existe una particion montada con el id " + id + ".";
        return false;
    }

    //crea estado temporal para leer users.txt
    AppState loginState = appState;
    loginState.session.active = true;
    loginState.session.user = "root";
    loginState.session.uid = 1;
    loginState.session.gid = 1;
    loginState.session.partitionId = id;

    FileManager fileManager;
    string content;
    string fileMessage;

    if (!fileManager.readFile("/users.txt", loginState, content, fileMessage)){
        message = "Error: no se pudo leer users.txt. " + fileMessage;
        return false;
    }

    int uid = -1;
    int gid = -1;

    if (!userExists(content, user)){
        message = "Error: el usuario no existe.";
        return false;
    }

    //verifica usuario y contraseña
    if (!validLogin(content, user, password, uid, gid)){
        message = "Error: usuario o contraseña incorrectos.";
        return false;
    }

    //guarda la sesion
    appState.session.active = true;
    appState.session.user = user;
    appState.session.uid = uid;
    appState.session.gid = gid;
    appState.session.partitionId = id;

    message = "Sesion iniciada correctamente con el usuario " + user + ".";
    return true;
}


//cierra la sesion actual
bool UserManager::logout(AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: no hay una sesion activa.";
        return false;
    }

    appState.session = Session();

    message = "Sesion cerrada correctamente.";
    return true;
}


//crea un grupo nuevo
bool UserManager::createGroup(const string& name, AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: debe iniciar sesion para crear un grupo.";
        return false;
    }

    if (!isRoot(appState)){
        message = "Error: solamente root puede crear grupos.";
        return false;
    }

    if (name.empty()){
        message = "Error: el nombre del grupo no puede estar vacio.";
        return false;
    }

    if (name.find_first_of(",\r\n") != string::npos){
        message = "Error: el grupo no puede contener separadores de users.txt.";
        return false;
    }

    if (name.size() > 10){
        message = "Error: el nombre del grupo no puede superar 10 caracteres.";
        return false;
    }

    string content;

    if (!readUsersFile(appState, content, message)){
        return false;
    }

    if (groupExists(content, name)){
        message = "Error: ya existe un grupo con el nombre " + name + ".";
        return false;
    }

    int groupId = getNextGroupId(content);

    content += to_string(groupId) + ",G," + name + "\n";

    if (!writeUsersFile(appState, content, message)){
        return false;
    }

    message = "Grupo " + name + " creado correctamente.";
    return true;
}


//elimina un grupo
bool UserManager::removeGroup(const string& name, AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: debe iniciar sesion para eliminar un grupo.";
        return false;
    }

    if (!isRoot(appState)){
        message = "Error: solamente root puede eliminar grupos.";
        return false;
    }

    if (name == "root"){
        message = "Error: no se puede eliminar el grupo root.";
        return false;
    }

    string content;

    if (!readUsersFile(appState, content, message)){
        return false;
    }

    if (!groupExists(content, name)){
        message = "Error: el grupo " + name + " no existe.";
        return false;
    }

    if (!markGroupDeleted(content, name)){
        message = "Error: no se pudo eliminar el grupo.";
        return false;
    }

    if (!writeUsersFile(appState, content, message)){
        return false;
    }

    message = "Grupo " + name + " eliminado correctamente.";
    return true;
}


//crea un usuario nuevo
bool UserManager::createUser(const string& user, const string& password, const string& group, AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: debe iniciar sesion para crear un usuario.";
        return false;
    }

    if (!isRoot(appState)){
        message = "Error: solamente root puede crear usuarios.";
        return false;
    }

    if (user.empty() || password.empty() || group.empty()){
        message = "Error: usuario, contraseña y grupo son obligatorios.";
        return false;
    }

    if (user.find_first_of(",\r\n") != string::npos || password.find_first_of(",\r\n") != string::npos || group.find_first_of(",\r\n") != string::npos){
        message = "Error: usuario, contraseña y grupo no pueden contener separadores de users.txt.";
        return false;
    }

    if (user.size() > 10 || password.size() > 10 || group.size() > 10){
        message = "Error: usuario, contraseña y grupo no pueden superar 10 caracteres.";
        return false;
    }

    string content;

    if (!readUsersFile(appState, content, message)){
        return false;
    }

    if (userExists(content, user)){
        message = "Error: ya existe un usuario con el nombre " + user + ".";
        return false;
    }

    if (!groupExists(content, group)){
        message = "Error: el grupo " + group + " no existe.";
        return false;
    }

    int userId = getNextUserId(content);

    content += to_string(userId) + ",U," + group + "," + user + "," + password + "\n";

    if (!writeUsersFile(appState, content, message)){
        return false;
    }

    message = "Usuario " + user + " creado correctamente.";
    return true;
}


//elimina un usuario
bool UserManager::removeUser(const string& user, AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: debe iniciar sesion para eliminar un usuario.";
        return false;
    }

    if (!isRoot(appState)){
        message = "Error: solamente root puede eliminar usuarios.";
        return false;
    }

    if (user == "root"){
        message = "Error: no se puede eliminar el usuario root.";
        return false;
    }

    string content;

    if (!readUsersFile(appState, content, message)){
        return false;
    }

    if (!userExists(content, user)){
        message = "Error: el usuario " + user + " no existe.";
        return false;
    }

    if (!markUserDeleted(content, user)){
        message = "Error: no se pudo eliminar el usuario.";
        return false;
    }

    if (!writeUsersFile(appState, content, message)){
        return false;
    }

    message = "Usuario " + user + " eliminado correctamente.";
    return true;
}


//cambia el grupo de un usuario
bool UserManager::changeUserGroup(const string& user, const string& group, AppState& appState, string& message) const {
    if (!hasActiveSession(appState)){
        message = "Error: debe iniciar sesion para cambiar el grupo de un usuario.";
        return false;
    }

    if (!isRoot(appState)){
        message = "Error: solamente root puede cambiar grupos de usuarios.";
        return false;
    }

    string content;

    if (!readUsersFile(appState, content, message)){
        return false;
    }

    if (!userExists(content, user)){
        message = "Error: el usuario " + user + " no existe.";
        return false;
    }

    if (!groupExists(content, group)){
        message = "Error: el grupo " + group + " no existe.";
        return false;
    }

    if (!updateUserGroup(content, user, group)){
        message = "Error: no se pudo cambiar el grupo del usuario.";
        return false;
    }

    if (!writeUsersFile(appState, content, message)){
        return false;
    }

    message = "Grupo del usuario " + user + " cambiado correctamente a " + group + ".";
    return true;
}


//lee todo el contenido de users.txt
bool UserManager::readUsersFile(const AppState& appState, string& content, string& message) const {
    FileManager fileManager;

    return fileManager.readFile("/users.txt", appState, content, message);
}


//escribe el nuevo contenido de users.txt
bool UserManager::writeUsersFile(AppState& appState, const string& content, string& message) const {
    FileManager fileManager;

    return fileManager.writeFile("/users.txt", content, appState, message);
}


//verifica si hay una sesion activa
bool UserManager::hasActiveSession(const AppState& appState) const {
    return appState.session.active;
}


//verifica si el usuario actual es root
bool UserManager::isRoot(const AppState& appState) const {
    return appState.session.active && appState.session.user == "root" && appState.session.uid == 1;
}


//busca un grupo activo
bool UserManager::groupExists(const string& content, const string& group) const {
    vector<string> lines = StringUtils::split(content, '\n');

    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 3){
            continue;
        }

        if (fields[0] != "0" && fields[1] == "G" && fields[2] == group){
            return true;
        }
    }

    return false;
}


//busca un usuario activo
bool UserManager::userExists(const string& content, const string& user) const {
    vector<string> lines = StringUtils::split(content, '\n');

    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 5){
            continue;
        }

        if (fields[0] != "0" && fields[1] == "U" && fields[3] == user){
            return true;
        }
    }

    return false;
}


//busca las credenciales de un usuario
bool UserManager::validLogin(const string& content, const string& user, const string& password, int& uid, int& gid) const {
    vector<string> lines = StringUtils::split(content, '\n');

    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 5){
            continue;
        }

        if (fields[0] == "0" || fields[1] != "U"){
            continue;
        }

        //usuario y contraseña respetan mayusculas
        if (fields[3] == user && fields[4] == password){
            uid = StringUtils::toPositiveInt(fields[0]);
            gid = getGroupId(content, fields[2]);

            if (uid <= 0 || gid <= 0){
                return false;
            }

            return true;
        }
    }

    return false;
}


//obtiene el id de un grupo
int UserManager::getGroupId(const string& content, const string& group) const {
    vector<string> lines = StringUtils::split(content, '\n');

    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 3){
            continue;
        }

        if (fields[0] != "0" && fields[1] == "G" && fields[2] == group){
            return StringUtils::toPositiveInt(fields[0]);
        }
    }

    return -1;
}


//obtiene el siguiente id para grupo
int UserManager::getNextGroupId(const string& content) const {
    vector<string> lines = StringUtils::split(content, '\n');
    int greaterId = 0;
    int recordCount = 0;

    //busca id mayor de grupos activos
    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 3 || fields[1] != "G"){
            continue;
        }

        recordCount++;
        int currentId = StringUtils::toPositiveInt(fields[0]);

        if (currentId > greaterId){
            greaterId = currentId;
        }
    }

    return max(greaterId, recordCount) + 1;
}


//obtiene el siguiente id para usuario
int UserManager::getNextUserId(const string& content) const {
    vector<string> lines = StringUtils::split(content, '\n');
    int greaterId = 0;
    int recordCount = 0;

    //busca id mayor de usuarios activos
    for (const string& line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() != 5 || fields[1] != "U"){
            continue;
        }

        recordCount++;
        int currentId = StringUtils::toPositiveInt(fields[0]);

        if (currentId > greaterId){
            greaterId = currentId;
        }
    }

    return max(greaterId, recordCount) + 1;
}


//marca un grupo como eliminado
bool UserManager::markGroupDeleted(string& content, const string& group) const {
    vector<string> lines = StringUtils::split(content, '\n');
    bool found = false;
    string newContent;

    for (string line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() == 3 && fields[0] != "0" && fields[1] == "G" && fields[2] == group){
            fields[0] = "0";
            line = fields[0] + "," + fields[1] + "," + fields[2];
            found = true;
        }

        newContent += line + "\n";
    }

    if (!found){
        return false;
    }

    content = newContent;
    return true;
}


//marca un usuario como eliminado
bool UserManager::markUserDeleted(string& content, const string& user) const {
    vector<string> lines = StringUtils::split(content, '\n');
    bool found = false;
    string newContent;

    for (string line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() == 5 && fields[0] != "0" && fields[1] == "U" && fields[3] == user){
            fields[0] = "0";
            line = fields[0] + "," + fields[1] + "," + fields[2] + "," + fields[3] + "," + fields[4];
            found = true;
        }

        newContent += line + "\n";
    }

    if (!found){
        return false;
    }

    content = newContent;
    return true;
}


//cambia el grupo dentro del registro del usuario
bool UserManager::updateUserGroup(string& content, const string& user, const string& group) const {
    vector<string> lines = StringUtils::split(content, '\n');
    bool found = false;
    string newContent;

    for (string line : lines){
        if (line.empty()){
            continue;
        }

        vector<string> fields = StringUtils::split(line, ',');

        if (fields.size() == 5 && fields[0] != "0" && fields[1] == "U" && fields[3] == user){
            fields[2] = group;
            line = fields[0] + "," + fields[1] + "," + fields[2] + "," + fields[3] + "," + fields[4];
            found = true;
        }

        newContent += line + "\n";
    }

    if (!found){
        return false;
    }

    content = newContent;
    return true;
}
