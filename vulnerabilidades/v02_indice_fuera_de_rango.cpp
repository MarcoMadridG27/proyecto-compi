// VULN: lectura con indice constante fuera de rango
// STATIC: Índice constante 7 fuera de rango
// DYNAMIC: bounds | acceso fuera de limites en arreglo
#include <iostream>
using namespace std;
int main() {
    int a[5];
    for (int i = 0; i < 5; i++) a[i] = i;
    cout << a[7] << endl;
    return 0;
}
