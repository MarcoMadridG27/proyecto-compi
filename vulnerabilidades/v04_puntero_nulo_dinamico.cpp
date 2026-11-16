// VULN: la funcion devuelve nullptr y el llamador lo desreferencia (solo se ve al ejecutar)
// DYNAMIC: null | desreferencia de puntero nulo
#include <iostream>
using namespace std;
int* buscar(int* v, int n, int x) {
    for (int i = 0; i < n; i++) {
        if (v[i] == x) return &v[i];
    }
    return nullptr;
}
int main() {
    int v[3];
    v[0] = 1; v[1] = 2; v[2] = 3;
    int* r = buscar(v, 3, 9);
    cout << *r << endl;
    return 0;
}
