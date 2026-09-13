#include "analysis/CommandParser.hpp"

#include <cctype>
using namespace std;

string CommandParser::toLower(const string& text) const {
    string lowerText = text;

    for (char& character : lowerText) {
        character = static_cast<char>(tolower(static_cast<unsigned char>(character)));
    }

    return lowerText;
}

ParseResult CommandParser::parse(const vector<Token>& tokens) const {
    ParseResult result;
    size_t tokenPos = 0;

    //recorre tokens y parsea
    while (tokenPos < tokens.size()) {

        //ignora fin de line
        while (tokenPos < tokens.size() && tokens[tokenPos].type == TokenType::EndOfLine) {
            tokenPos++;
        }

        //si se acaban los tokens muere
        if (tokenPos >= tokens.size()) {
            break;
        }

        //si es comentario lo guarda
        if (tokens[tokenPos].type == TokenType::Comment) {
            ParsedCommand parsedComment;
            parsedComment.isComment = true;
            parsedComment.commentText = tokens[tokenPos].text;
            result.commands.push_back(parsedComment);
            tokenPos++;

            //ignora hasta siguiente linea
            while (tokenPos < tokens.size() && tokens[tokenPos].type != TokenType::EndOfLine) {
                tokenPos++;
            }

            continue;
        }

        //si es invalid muere
        if (tokens[tokenPos].type == TokenType::Invalid) {
            //el lexer ya informo este error

            while (tokenPos < tokens.size() && tokens[tokenPos].type != TokenType::EndOfLine) {
                tokenPos++;
            }

            continue;
        }

        //si no es word muere
        if (tokens[tokenPos].type != TokenType::Word) {
            result.errors.push_back("Error sintactico: se esperaba el nombre de un comando.");

            while (tokenPos < tokens.size() && tokens[tokenPos].type != TokenType::EndOfLine) {
                tokenPos++;
            }

            continue;
        }

        //comando parseado
        ParsedCommand parsedCommand;
        parsedCommand.name = toLower(tokens[tokenPos].text);
        tokenPos++;

        bool commandHasError = false;

        const string knownCommands = " mkdisk rmdisk fdisk mount mounted mkfs cat login logout mkgrp rmgrp mkusr rmusr chgrp mkfile mkdir rep ";
        if (knownCommands.find(" " + parsedCommand.name + " ") == string::npos){
            result.errors.push_back("Error sintactico: comando no reconocido " + parsedCommand.name + ".");
            commandHasError = true;
        }

        //recorre y parsea param
        while (!commandHasError && tokenPos < tokens.size() && tokens[tokenPos].type != TokenType::EndOfLine) {

            //si aparece comentario termina comando
            if (tokens[tokenPos].type == TokenType::Comment) {
                break;
            }

            if (tokens[tokenPos].type != TokenType::Parameter || !tokens[tokenPos].separated) {
                result.errors.push_back("Error sintactico en " + parsedCommand.name + ": se esperaba un parametro.");
                commandHasError = true;
                break;
            }

            ParsedParam parsedParam;
            string completeParam = tokens[tokenPos].text;

            //quita el - del parametro
            parsedParam.name = toLower(completeParam.substr(1));
            parsedParam.value = "";
            parsedParam.hasValue = false;

            tokenPos++;

            //si = busca valor
            if (tokenPos < tokens.size() && tokens[tokenPos].type == TokenType::Equal) {
                tokenPos++;

                //si no existe valor da error
                if (tokenPos >= tokens.size() || tokens[tokenPos].type == TokenType::EndOfLine || tokens[tokenPos].type == TokenType::Comment || (tokens[tokenPos].type != TokenType::Word && tokens[tokenPos].type != TokenType::QuotedValue)) {
                    result.errors.push_back("Error sintactico en " + parsedCommand.name + ": falta el valor de -" + parsedParam.name + ".");
                    commandHasError = true;
                    break;
                }

                //si valid lo guarda
                parsedParam.value = tokens[tokenPos].text;
                parsedParam.hasValue = true;
                tokenPos++;
            }

            parsedCommand.params.push_back(parsedParam);
        }

        //si error ignora hasta finln
        if (commandHasError) {
            while (tokenPos < tokens.size() && tokens[tokenPos].type != TokenType::EndOfLine) {
                tokenPos++;
            }
        } else {
            result.commands.push_back(parsedCommand);
        }

        //si hay comentario despues del comando lo guarda
        if (tokenPos < tokens.size() && tokens[tokenPos].type == TokenType::Comment) {
            ParsedCommand parsedComment;
            parsedComment.isComment = true;
            parsedComment.commentText = tokens[tokenPos].text;
            result.commands.push_back(parsedComment);
            tokenPos++;
        }

        //si finln ignora
        if (tokenPos < tokens.size() && tokens[tokenPos].type == TokenType::EndOfLine) {
            tokenPos++;
        }
    }

    return result;
}
