// VULN: la funcion no retorna valor en todos los caminos
// STATIC: puede terminar sin retornar un valor
#include <iostream>
using namespace std;
int signo(int x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
}
int main() {
    cout << signo(5) << endl;
    return 0;
}
