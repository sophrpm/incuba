#ifndef CAT_COMMAND_HPP
#define CAT_COMMAND_HPP

#include <string>
#include <vector>

#include "analysis/ParsedCommand.hpp"
#include "state/AppState.hpp"
#include "validation/ValidationResult.hpp"

using namespace std;


//maneja el comando cat
class CatCommand {
public:

    //ejecuta cat
    ValidationResult execute(const ParsedCommand& command, const AppState& appState) const;

private:

    //obtiene los archivos enviados como -file1, -file2...
    vector<string> getFiles(const ParsedCommand& command) const;
};


#endif
