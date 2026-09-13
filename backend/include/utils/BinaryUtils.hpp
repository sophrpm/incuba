#ifndef BINARY_UTILS_HPP
#define BINARY_UTILS_HPP

#include <fstream>
#include <string>

using namespace std;


//utilidades para leer y escribir archivos binarios
class BinaryUtils {
public:

    //lee una estructura desde una posicion del archivo
    template <typename T>
    static bool readStruct(const string& path, int position, T& data){
        fstream file(path, ios::in | ios::binary);

        if (!file.is_open()){
            return false;
        }

        if (position < 0 || static_cast<long long>(position) + static_cast<long long>(sizeof(T)) > getFileSize(path)){
            return false;
        }

        file.seekg(position, ios::beg);

        if (!file.good()){
            file.close();
            return false;
        }

        file.read(reinterpret_cast<char*>(&data), sizeof(T));

        bool readOk = file.good();
        file.close();

        return readOk;
    }

    //escribe una estructura en una posicion del archivo
    template <typename T>
    static bool writeStruct(const string& path, int position, const T& data){
        fstream file(path, ios::in | ios::out | ios::binary);

        if (!file.is_open()){
            return false;
        }

        if (position < 0 || static_cast<long long>(position) + static_cast<long long>(sizeof(T)) > getFileSize(path)){
            return false;
        }

        file.seekp(position, ios::beg);

        if (!file.good()){
            file.close();
            return false;
        }

        file.write(reinterpret_cast<const char*>(&data), sizeof(T));

        bool writeOk = file.good();
        file.close();

        return writeOk;
    }

    //llena un espacio del archivo con ceros
    static bool clearSpace(const string& path, int start, int size);

    //verifica si existe un archivo
    static bool fileExists(const string& path);

    //obtiene el tamaño de un archivo
    static long long getFileSize(const string& path);

    //copia texto a un arreglo char fijo
    static void copyToFixedChar(char* destination, int size, const string& text);

    //convierte arreglo char fijo a string
    static string fixedCharToString(const char* text, int size);
};


#endif
