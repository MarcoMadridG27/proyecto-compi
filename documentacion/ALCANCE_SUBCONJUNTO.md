# Alcance del subconjunto de C++ y relación con el estándar

Lenguaje replicado: **C++17**, ISO/IEC 14882:2017 (borrador de trabajo N4659). La gramática del subconjunto está en
[`gramatica.ebnf`](../gramatica.ebnf). Un programa válido del subconjunto debe compilarse y producir el mismo resultado
que con `g++`; los resultados esperados de las pruebas salen de `g++ -std=c++14 -fno-elide-constructors`, la semántica de
copia que implementa el compilador (sin elisión ni movimiento).

Las referencias usan el identificador estable del estándar y el número de cláusula de N4659.
La ABI sigue *System V AMD64* y las reglas de clases no triviales de la *Itanium C++ ABI*.

| Característica | Estado y justificación | Estándar | Observación |
|---|---|---|---|
| Comentarios, identificadores, palabras clave | Incluido: base de cualquier programa | `[lex.comment]` 5.4, `[lex.name]` 5.10, `[lex.key]` 5.11 | Línea y columna en cada token |
| Literales | Incluido: enteros, reales, `char`, cadenas, `true`/`false`, `nullptr` | `[lex.icon]` 5.13.2, `[lex.ccon]` 5.13.3, `[lex.fcon]` 5.13.4, `[lex.string]` 5.13.5, `[lex.nullptr]` 5.13.7 | Sin sufijos (`1.5f`) ni `long long` |
| Tipos fundamentales | Incluido: `int`, `float`, `double`, `bool`, `char`, `void` | `[basic.fundamental]` 6.9.1 | Modelo de datos LP64 |
| Punteros, referencias, arreglos | Incluido: base del lenguaje y de la ABI | `[dcl.ptr]` 11.3.1, `[dcl.ref]` 11.3.2, `[dcl.array]` 11.3.4 | Una dimensión en `new[]` |
| `const` | Incluido completo: variables, punteros, referencias, métodos | `[dcl.type.cv]` 10.1.7.1, `[conv.qual]` 7.5 | Sin `volatile` ni `mutable` |
| Conversiones estándar | Incluido: promociones, integrales, flotante-entero, booleanas, de puntero | `[conv.prom]` 7.6, `[conv.integral]` 7.8, `[conv.fpint]` 7.10, `[conv.bool]` 7.14 | Sin conversiones definidas por el usuario |
| Operadores | Incluido: aritméticos, relacionales, lógicos con cortocircuito, bit a bit, desplazamientos, asignación, `++`/`--` | `[expr.mul]` 8.6, `[expr.add]` 8.7, `[expr.shift]` 8.8, `[expr.log.and]` 8.14, `[expr.ass]` 8.18 | Sin `?:` ni asignación compuesta |
| Sentencias | Incluido: `if`, `while`, `do`, `for`, `break`, `continue`, `return` | `[stmt.if]` 9.4.1, `[stmt.while]` 9.5.1, `[stmt.for]` 9.5.3, `[stmt.break]` 9.6.1 | Sin `switch` ni `goto` |
| Funciones | Incluido: recursión, paso por valor, puntero y referencia, retorno por valor | `[dcl.fct]` 11.3.5, `[basic.start.main]` 6.6.1 | Sin prototipos sin cuerpo, sobrecarga ni argumentos por defecto |
| Clases y `struct` | Incluido: campos, métodos, `this`, `public`/`private` | `[class.mem]` 12.2, `[class.mfct]` 12.2.1, `[class.access]` cláusula 14 | Sin `static`, `friend`, `protected` |
| Constructores y destructor | Incluido: por defecto, con parámetros, de copia, listas de inicialización, inicializadores de campo | `[class.ctor]` 15.1, `[class.dtor]` 15.4, `[class.base.init]` 15.6.2 | Un constructor con parámetros por clase |
| Asignación de copia | Incluido: `T& operator=(const T&)` y la implícita | `[class.copy.assign]` 15.8.2, `[over.ass]` 16.5.3 | Única sobrecarga de operador |
| Memoria dinámica | Incluido: `new`, `new[]`, `delete`, `delete[]` | `[expr.new]` 8.3.4, `[expr.delete]` 8.3.5 | Sin `placement new` |
| Inicialización | Incluido: directa, de copia, con llaves de agregados | `[dcl.init]` 11.6, `[dcl.init.aggr]` 11.6.1 | Sin `std::initializer_list` |
| Biblioteca | Mínimo: `<iostream>` (`cout`, `cin`, `endl`), `<string>` (métodos básicos), `<cmath>` (`sqrt`) | `[iostream.objects]`, `[string.classes]`, `[c.math]` | El runtime propio no usa libstdc++ |
| Herencia, virtuales, plantillas, excepciones, RTTI, espacios de nombres propios | Excluido: no aportan a los objetivos de optimización y seguridad y multiplican el alcance | cláusulas 13, 17, 18 | Se rechazan con mensaje claro |

## Diferencias deliberadas respecto a C++17

- Sin constructor ni asignación de movimiento ni elisión de copias: `T t = f();` hace las dos copias de C++14.
- Un literal de cadena se acepta como `char*` (extensión de GCC) además de `const char*`.
- Las conversiones implícitas por constructor (`T t = 5;`) se rechazan.
