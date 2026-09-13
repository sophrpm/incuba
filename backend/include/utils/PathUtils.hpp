#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>

using namespace std;


//utilidades para trabajar con rutas
class PathUtils {
public:
    static string canonicalPath(const string& path);

    //crea las carpetas necesarias para una ruta
    static bool createDirectories(const string& path);

    //obtiene la carpeta padre de una ruta
    static string getParentPath(const string& path);

    //obtiene el nombre del archivo
    static string getFileName(const string& path);

    //obtiene la extension del archivo
    static string getExtension(const string& path);

    //verifica si una ruta termina en una extension
    static bool hasExtension(const string& path, const string& extension);

    //verifica si la ruta es absoluta
    static bool isAbsolute(const string& path);
};


#endif
