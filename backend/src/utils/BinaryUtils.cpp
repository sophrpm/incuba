#include "utils/BinaryUtils.hpp"

#include <fstream>
#include <cstring>

using namespace std;


//llena un espacio del archivo con ceros
bool BinaryUtils::clearSpace(const string& path, int start, int size){
    if (start < 0 || size < 0 || static_cast<long long>(start) + size > getFileSize(path)){
        return false;
    }

    fstream file(path, ios::in | ios::out | ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.seekp(start, ios::beg);

    if (!file.good()){
        file.close();
        return false;
    }

    char buffer[1024] = {};
    int remainingBytes = size;

    //escribe ceros hasta completar el espacio
    while (remainingBytes > 0){
        int bytesToWrite = remainingBytes > 1024 ? 1024 : remainingBytes;
        file.write(buffer, bytesToWrite);

        if (!file.good()){
            file.close();
            return false;
        }

        remainingBytes -= bytesToWrite;
    }

    file.close();
    return true;
}


//verifica si existe un archivo
bool BinaryUtils::fileExists(const string& path){
    ifstream file(path, ios::binary);

    if (!file.is_open()){
        return false;
    }

    file.close();
    return true;
}


//obtiene el tamaño de un archivo
long long BinaryUtils::getFileSize(const string& path){
    ifstream file(path, ios::binary | ios::ate);

    if (!file.is_open()){
        return -1;
    }

    long long fileSize = static_cast<long long>(file.tellg());

    file.close();
    return fileSize;
}


//copia texto a un arreglo char fijo
void BinaryUtils::copyToFixedChar(char* destination, int size, const string& text){
    if (destination == nullptr || size <= 0){
        return;
    }

    //limpia primero todo el arreglo
    for (int position = 0; position < size; position++){
        destination[position] = '\0';
    }

    int charactersToCopy = static_cast<int>(text.size());

    if (charactersToCopy > size){
        charactersToCopy = size;
    }

    //copia solamente lo que cabe
    for (int position = 0; position < charactersToCopy; position++){
        destination[position] = text[position];
    }
}


//convierte arreglo char fijo a string
string BinaryUtils::fixedCharToString(const char* text, int size){
    if (text == nullptr || size <= 0){
        return "";
    }

    string result;

    //lee hasta tamaño maximo o caracter nulo
    for (int position = 0; position < size; position++){
        if (text[position] == '\0'){
            break;
        }

        result += text[position];
    }

    return result;
}
