// VULN: lectura de una variable sin inicializar en un camino
// STATIC: se usa sin haber sido inicializada
#include <iostream>
using namespace std;
int main() {
    int n;
    int x;
    cin >> n;
    if (n > 0) {
        x = 1;
    }
    cout << x << endl;
    return 0;
}
