#ifndef API_H
#define API_H

#include <string>
#include <vector>
#include "security.h"

using namespace std;

// Opciones del modo API: el codigo llega por stdin y la salida es un solo objeto JSON
struct ApiOptions {
    string mode = "all";           // scan, ast, check, asm, all
    bool optimize = false;         // -O1
    vector<string> passes;         // inline, cprop, fold, strength, dce, sethi, peephole
    SecurityOptions sec;           // mecanismos de seguridad (--sec=...)
    vector<string> optNotes;       // avisos del driver sobre pases desactivados
};

// Ejecuta el pipeline sobre el codigo fuente y devuelve el JSON de respuesta
string runApi(const string& source, const ApiOptions& opts);

// Resultado del pipeline de compilacion compartido por el modo API y el CLI
struct OptimizerReportEntry {
    string name;
    int count = 0;
    vector<string> details;
};

#endif // API_H
