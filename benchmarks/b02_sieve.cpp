// Benchmark: criba de Eratostenes (arreglo global, acceso por indice, bucles anidados)
#include <iostream>
using namespace std;
int marca[12000001];
int main() {
    int n = 12000000;
    for (int i = 0; i <= n; i++) marca[i] = 1;
    marca[0] = 0;
    marca[1] = 0;
    for (int i = 2; i * i <= n; i++) {
        if (marca[i] == 1) {
            for (int j = i * i; j <= n; j = j + i) marca[j] = 0;
        }
    }
    int cuenta = 0;
    for (int i = 0; i <= n; i++) cuenta = cuenta + marca[i];
    cout << cuenta << endl;
    return 0;
}
