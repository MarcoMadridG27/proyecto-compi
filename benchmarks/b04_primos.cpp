// Benchmark: conteo de primos por division (operaciones / y %, condiciones, bucles)
#include <iostream>
using namespace std;
bool esPrimo(int n) {
    if (n < 2) return false;
    for (int d = 2; d * d <= n; d++) {
        if (n % d == 0) return false;
    }
    return true;
}
int main() {
    int cuenta = 0;
    for (int n = 0; n < 1500000; n++) {
        if (esPrimo(n)) cuenta++;
    }
    cout << cuenta << endl;
    return 0;
}
