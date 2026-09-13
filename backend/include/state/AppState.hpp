#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <string>
#include <vector>

#include "state/MountedPartition.hpp"
#include "state/Session.hpp"

using namespace std;


//guarda una pregunta pendiente, no estructuras de ext2
struct PendingConfirmation {
    bool active = false;
    string path;
    string content;
    bool recursive = false;
    string remainingInput;
    int successfulCommands = 0;
    int lexicalErrors = 0;
    int syntaxErrors = 0;
    bool allSuccess = true;
};

//guarda el estado general de la aplicacion
struct AppState {
    string carnet = "202405047";
    vector<MountedPartition> mountedPartitions;
    Session session;
    PendingConfirmation pendingConfirmation;
};


#endif
