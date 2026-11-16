// VULN: se devuelve la direccion de una variable local
// STATIC: Puntero colgante
#include <iostream>
using namespace std;
int* crear() {
    int x = 3;
    return &x;
}
int main() {
    int* p = crear();
    cout << "creado" << endl;
    return 0;
}
