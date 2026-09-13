#ifndef COMMAND_PARSER_HPP
#define COMMAND_PARSER_HPP

#include <string>
#include <vector>

#include "analysis/CommandToken.hpp"
#include "analysis/ParsedCommand.hpp"

using namespace std;

//guarda los comandos obtenidos y los errores encontrados durante el parseo
struct ParseResult {
    vector<ParsedCommand> commands;
    vector<string> errors;
};

//convierte la lista de tokens en comandos con sus parametros
class CommandParser {
public:
    ParseResult parse(const vector<Token>& tokens) const;

private:
    //convierte nombres de comandos y parametros a minuscula
    string toLower(const string& text) const;
};

#endif
