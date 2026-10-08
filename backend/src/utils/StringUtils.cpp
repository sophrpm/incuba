#include "utils/StringUtils.hpp"

#include <cctype>

using namespace std;


//convierte texto a minuscula
string StringUtils::toLower(const string& text){
    string lowerText = text;

    for (char& character : lowerText){
        character = static_cast<char>(tolower(static_cast<unsigned char>(character)));
    }

    return lowerText;
}


//convierte texto a mayuscula
string StringUtils::toUpper(const string& text){
    string upperText = text;

    for (char& character : upperText){
        character = static_cast<char>(toupper(static_cast<unsigned char>(character)));
    }

    return upperText;
}


//compara dos textos ignorando mayusculas
bool StringUtils::equalsIgnoreCase(const string& firstText, const string& secondText){
    return toLower(firstText) == toLower(secondText);
}


//quita espacios al inicio y final
string StringUtils::trim(const string& text){
    size_t start = 0;
    size_t end = text.size();

    //busca primer caracter que no sea espacio
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '\r' || text[start] == '\n')){
        start++;
    }

    //busca ultimo caracter que no sea espacio
    while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n')){
        end--;
    }

    return text.substr(start, end - start);
}


//separa texto usando un delimitador
vector<string> StringUtils::split(const string& text, char delimiter){
    vector<string> parts;
    string currentPart;

    //recorre el texto y forma cada parte
    for (char character : text){
        if (character == delimiter){
            parts.push_back(currentPart);
            currentPart = "";
            continue;
        }

        currentPart += character;
    }

    //guarda la ultima parte
    parts.push_back(currentPart);

    return parts;
}


//verifica si el texto esta vacio
bool StringUtils::isEmpty(const string& text){
    return trim(text).empty();
}

//convierte ids sin excepciones
int StringUtils::toPositiveInt(const string& text){
    if (text.empty()){
        return -1;
    }
    int value = 0;
    for (char character : text){
        if (character < '0' || character > '9' || value > (2147483647 - (character - '0')) / 10){
            return -1;
        }
        value = value * 10 + character - '0';
    }
    return value;
}

//check de texto utf8, no muestra binarios como si fueran texto
bool StringUtils::isText(const string& content){
    size_t position = 0;
    while (position < content.size()){
        unsigned char character = content[position++];
        if (character < 128){
            if ((character < 32 && character != '\n' && character != '\r' && character != '\t') || character == 127) return false;
            continue;
        }
        int following;
        unsigned int code;
        unsigned int minimum;
        if (character >= 194 && character <= 223){ following = 1; code = character & 31; minimum = 128; }
        else if (character >= 224 && character <= 239){ following = 2; code = character & 15; minimum = 2048; }
        else if (character >= 240 && character <= 244){ following = 3; code = character & 7; minimum = 65536; }
        else return false;
        if (position + following > content.size()) return false;
        for (int i = 0; i < following; i++){
            unsigned char next = content[position++];
            if (next < 128 || next > 191) return false;
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
    }
    return true;
}
