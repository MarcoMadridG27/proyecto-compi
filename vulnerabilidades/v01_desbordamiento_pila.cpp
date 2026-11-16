// VULN: desbordamiento de buffer en la pila (escritura una posicion mas alla en un bucle)
// STATIC: llega a 6
// DYNAMIC: bounds | acceso fuera de limites en arreglo
// DYNAMIC: canary | desbordamiento de pila detectado por el canario
#include <iostream>
using namespace std;
int main() {
    int a[4];
    for (int i = 0; i <= 6; i++) {
        a[i] = i * 10;
    }
    cout << a[0] << endl;
    return 0;
}
