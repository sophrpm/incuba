#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

#include <string>
#include <vector>

using namespace std;


//utilidades para trabajar con texto
class StringUtils {
public:
    static int toPositiveInt(const string& text);

    //convierte texto a minuscula
    static string toLower(const string& text);

    //convierte texto a mayuscula
    static string toUpper(const string& text);

    //compara dos textos ignorando mayusculas
    static bool equalsIgnoreCase(const string& firstText, const string& secondText);

    //quita espacios al inicio y final
    static string trim(const string& text);

    //separa texto usando un delimitador
    static vector<string> split(const string& text, char delimiter);

    //verifica si el texto esta vacio
    static bool isEmpty(const string& text);
};


#endif
