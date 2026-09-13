#include "analysis/CommandToken.hpp"

#include <cctype>
using namespace std;

// verifica separadores
bool CommandToken::isSeparator(char character) const {
    return character == ' ' || character == '\t' || character == '\r' || character == '\n' || character == '=' || character == '#';
}

//funcion que tokeniza entrada y devuelve res...
TokenizeResult CommandToken::tokenize(const string& inputText) const {
    TokenizeResult result;
    size_t textPos = 0;

    //bucle que recorre el txt y toknisa
    while (textPos < inputText.size()) {
        char currentCharacter = inputText[textPos];

        //ignora espacios
        if (currentCharacter == ' ' || currentCharacter == '\t' || currentCharacter == '\r') {
            textPos++;
            continue;
        }

        //salto de linea:)
        if (currentCharacter == '\n') {
            result.tokens.push_back({TokenType::EndOfLine, "\n"});
            textPos++;
            continue;
        }

        //guarda linea de comentario
        if (currentCharacter == '#') {
            string commentText;

            while (textPos < inputText.size() && inputText[textPos] != '\n') {
                commentText += inputText[textPos];
                textPos++;
            }

            result.tokens.push_back({TokenType::Comment, commentText});
            continue;
        }

        //singo igual=
        if (currentCharacter == '=') {
            result.tokens.push_back({TokenType::Equal, "="});
            textPos++;
            continue;
        }

        //comilas dobles"value"
        if (currentCharacter == '"') {
            textPos++;
            string quotedText;
            bool closedQuote = false;

            while (textPos < inputText.size()) {
                if (inputText[textPos] == '"') {
                    closedQuote = true;
                    textPos++;
                    break;
                }

                if (inputText[textPos] == '\n') {
                    break;
                }

                quotedText += inputText[textPos];
                textPos++;
            }

            if (!closedQuote) {
                result.errors.push_back("Error lexico: cadena entre comillas sin cerrar.");
                result.tokens.push_back({TokenType::Invalid, quotedText});
            } else {
                result.tokens.push_back({TokenType::QuotedValue, quotedText});
            }

            continue;
        }

        bool afterEqual = !result.tokens.empty() && result.tokens.back().type == TokenType::Equal;
        //signo de param-
        if (!afterEqual && currentCharacter == '-' && textPos + 1 < inputText.size() && isalpha(static_cast<unsigned char>(inputText[textPos + 1]))) {
            bool separated = textPos == 0 || isspace(static_cast<unsigned char>(inputText[textPos - 1]));
            string parameterName;
            parameterName += inputText[textPos];
            textPos++;

            //este verifica param bueno
            while (textPos < inputText.size()) {
                char parameterCharacter = inputText[textPos];

                if (!isalnum(static_cast<unsigned char>(parameterCharacter)) && parameterCharacter != '_') {
                    break;
                }

                parameterName += parameterCharacter;
                textPos++;
            }

            result.tokens.push_back({TokenType::Parameter, parameterName, separated});
            continue;
        }

        string wordText;
        bool invalidWord = false;

        //contiene palabra
        while (textPos < inputText.size() && !isSeparator(inputText[textPos])) {
            if (inputText[textPos] == '"') {
                break;
            }

            unsigned char value = static_cast<unsigned char>(inputText[textPos]);
            if (value < 32 || value == 127 || inputText[textPos] == ';' || inputText[textPos] == '|' || inputText[textPos] == '`'){
                invalidWord = true;
            }
            wordText += inputText[textPos];
            textPos++;
        }

        if (!wordText.empty()) {
            if (invalidWord){
                result.errors.push_back("Error lexico: simbolo no permitido en una palabra.");
            }
            result.tokens.push_back({invalidWord ? TokenType::Invalid : TokenType::Word, wordText});
            continue;
        }

        //errres
        result.errors.push_back(string("Error lexico: simbolo no reconocido '") + currentCharacter + "'.");
        result.tokens.push_back({TokenType::Invalid, string(1, currentCharacter)});
        textPos++;
    }

    return result;
}
