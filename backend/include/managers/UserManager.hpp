#ifndef USER_MANAGER_HPP
#define USER_MANAGER_HPP

#include <string>

using namespace std;

struct AppState;


//maneja usuarios, grupos y sesion activa
class UserManager {
public:

    //inicia sesion con un usuario
    bool login(const string& user, const string& password, const string& id, AppState& appState, string& message) const;

    //cierra la sesion actual
    bool logout(AppState& appState, string& message) const;

    //crea un grupo nuevo
    bool createGroup(const string& name, AppState& appState, string& message) const;

    //elimina un grupo
    bool removeGroup(const string& name, AppState& appState, string& message) const;

    //crea un usuario nuevo
    bool createUser(const string& user, const string& password, const string& group, AppState& appState, string& message) const;

    //elimina un usuario
    bool removeUser(const string& user, AppState& appState, string& message) const;

    //cambia el grupo de un usuario
    bool changeUserGroup(const string& user, const string& group, AppState& appState, string& message) const;

private:

    //lee todo el contenido de users.txt
    bool readUsersFile(const AppState& appState, string& content, string& message) const;

    //escribe el nuevo contenido de users.txt
    bool writeUsersFile(AppState& appState, const string& content, string& message) const;

    //verifica si hay una sesion activa
    bool hasActiveSession(const AppState& appState) const;

    //verifica si el usuario actual es root
    bool isRoot(const AppState& appState) const;

    //busca un grupo activo
    bool groupExists(const string& content, const string& group) const;

    //busca un usuario activo
    bool userExists(const string& content, const string& user) const;

    //busca las credenciales de un usuario
    bool validLogin(const string& content, const string& user, const string& password, int& uid, int& gid) const;

    //obtiene el id de un grupo
    int getGroupId(const string& content, const string& group) const;

    //obtiene el siguiente id para grupo
    int getNextGroupId(const string& content) const;

    //obtiene el siguiente id para usuario
    int getNextUserId(const string& content) const;

    //marca un grupo como eliminado
    bool markGroupDeleted(string& content, const string& group) const;

    //marca un usuario como eliminado
    bool markUserDeleted(string& content, const string& user) const;

    //cambia el grupo dentro del registro del usuario
    bool updateUserGroup(string& content, const string& user, const string& group) const;
};


#endif
