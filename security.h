#ifndef SECURITY_H
#define SECURITY_H

#include <string>
#include <vector>

using namespace std;

// Mecanismos de seguridad del compilador. Todos se activan con --sec=lista (o --sec=all):
//   static    analisis estatico en tiempo de compilacion (advertencias de seguridad)
//   bounds    verificacion de limites en accesos a arreglos
//   null      verificacion de punteros nulos antes de desreferenciar
//   div       division entre cero
//   overflow  desbordamiento aritmetico en enteros (+, -, *, negacion, INT_MIN / -1)
//   canary    canario de pila en cada funcion
//   heap      asignador instrumentado: doble liberacion, uso despues de liberar y
//             desbordamiento de heap
//   recursion limite de profundidad de recursion
//   harden    endurecimiento del binario: codigo independiente de la posicion (PIE), pila no
//             ejecutable y RELRO completo (se aplica al enlazar, ver hardenLinkFlags)
struct SecurityOptions {
    bool statik = false;
    bool bounds = false;
    bool nullchk = false;
    bool divzero = false;
    bool overflow = false;
    bool canary = false;
    bool heap = false;
    bool recursion = false;
    bool harden = false;
    int maxDepth = 20000;

    bool anyDynamic() const { return bounds || nullchk || divzero || overflow || canary || heap || recursion; }
    bool any() const { return statik || anyDynamic(); }
};

// Tipos de violacion en tiempo de ejecucion; deben coincidir con rt_fail en runtime/runtime.c
enum SecurityKind {
    SEC_BOUNDS = 1,
    SEC_NULL = 2,
    SEC_DIV_ZERO = 3,
    SEC_OVERFLOW = 4,
    SEC_STACK_SMASH = 5,
    SEC_RECURSION = 6,
    SEC_DOUBLE_FREE = 7,
    SEC_USE_AFTER_FREE = 8,
    SEC_HEAP_OVERFLOW = 9,
    SEC_INVALID_FREE = 10
};

// Interpreta "a,b,c" o "all". Devuelve false si hay un nombre desconocido (bad lo recibe).
bool parseSecurityList(const string& list, SecurityOptions& out, string& bad);

// Banderas de enlace del endurecimiento del binario (gcc programa.s runtime.c <banderas>)
const char* hardenLinkFlags();

#endif  // SECURITY_H
