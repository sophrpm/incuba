#ifndef COMMAND_TOKEN_HPP
#define COMMAND_TOKEN_HPP

#include <string>
#include <vector>

using namespace std;

//tipos de elementos que puede reconocer el tokenizer
enum class TokenType {
    Word,
    Parameter,
    Equal,
    QuotedValue,
    EndOfLine,
    Invalid,
    Comment,
};

//guarda el tipo de token y el texto que representa
struct Token {
    TokenType type;
    string text;
    bool separated = true;
};

//resultado de tokenizar todo el texto recibido
struct TokenizeResult {
    vector<Token> tokens;
    vector<string> errors;
};

//recorre el texto y lo separa en tokens
class CommandToken {
public:
    TokenizeResult tokenize(const string& inputText) const;

private:
    //indica qué caracteres terminan una palabra
    bool isSeparator(char character) const;
};

#endif
