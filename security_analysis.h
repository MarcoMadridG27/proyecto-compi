#ifndef SECURITY_ANALYSIS_H
#define SECURITY_ANALYSIS_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include "ast.h"
#include "type_checker.h"

using namespace std;

// Analisis estatico de seguridad (tiempo de compilacion). Corre despues del TypeChecker,
// lee las anotaciones del AST y produce advertencias con fase "seguridad":
//   - variables escalares usadas sin inicializar (asignacion definitiva por flujo)
//   - funciones no void que pueden terminar sin retornar valor
//   - indices constantes (o de bucles for acotados) fuera del rango de un arreglo
//   - desbordamiento y division entre cero en expresiones constantes
//   - desreferencia de punteros nulos evidentes y uso despues de delete
//   - punteros colgantes: direccion de una variable local que escapa de su vida
//   - conversiones implicitas inseguras (perdida de informacion)
class SecurityAnalyzer {
public:
    vector<Diagnostic> findings;
    void analyze(Program* prog);

private:
    enum FactKind { F_NULL, F_FREED, F_ADDR_LOCAL, F_DANGLING };
    struct Fact {
        FactKind kind = F_NULL;
        int depth = 0;       // F_ADDR_LOCAL: profundidad del bloque de la variable apuntada
        string var;          // nombre de la variable apuntada (F_ADDR_LOCAL / F_DANGLING)
    };
    struct State {
        bool dead = false;                  // codigo inalcanzable: no restringe nada
        set<int> assigned;                  // locales sin inicializador ya asignadas
        map<int, Fact> facts;               // hechos sobre punteros locales
    };
    struct LoopCtx {
        bool hasBreakState = false;
        State breakState;
    };

    State cur;
    set<int> tracked;                       // locales escalares declaradas sin inicializador
    map<int, int> declDepth;                // profundidad de bloque de cada local
    map<int, string> slotName;
    set<int> reportedUninit;
    vector<LoopCtx> loops;
    map<int, pair<long long, long long>> loopRange;   // variable de bucle -> [minimo, maximo]
    set<string> seen;
    FunDecl* curFun = nullptr;
    int depth = 0;

    void warn(int line, int col, const string& msg);
    static State join(const State& a, const State& b);

    void function(FunDecl* f);
    void stmt(ASTNode* n);
    void block(BlockStmt* b);
    void varDecl(VarDecl* d);
    void expr(Exp* e);
    void lvalue(Exp* e);
    void exprList(const vector<Exp*>& v);
    void assignTo(Exp* target, Exp* value, int line, int col);
    void readLocal(IdExp* id);
    void derefCheck(Exp* pointer, int line, int col, const char* what);
    void leaveScope();
    void checkIndex(PostfixExp* pf);
    void checkNarrow(const Type& to, Exp* from, int line, int col);
    bool constInt(Exp* e, long long& v, bool report);
    bool alwaysReturns(ASTNode* n);
    bool hasBreak(ASTNode* n);
    bool assignsSlot(ASTNode* n, int slot);
    bool expAssignsSlot(Exp* e, int slot);
    void loopFor(ForStmt* f);
    void killFacts(State& s);
    bool linearInLoopVar(Exp* e, int& slot, long long& off);
    bool isNullLiteral(Exp* e);
    void guard(Exp* cond, bool whenTrue, vector<int>& nonNull);
    void assumeNonNull(const vector<int>& slots);
    static bool isLocal(const IdExp* id);
};

#endif  // SECURITY_ANALYSIS_H
