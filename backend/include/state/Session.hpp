#ifndef SESSION_HPP
#define SESSION_HPP

#include <string>

using namespace std;


//guarda los datos de la sesion actual
struct Session {
    bool active = false;
    string user = "";
    int uid = -1;
    int gid = -1;
    string partitionId = "";
};


#endif
