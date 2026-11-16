// VULN: division entre cero con divisor que llega por parametro
// DYNAMIC: div | division entre cero
#include <iostream>
using namespace std;
int dividir(int a, int b) {
    return a / b;
}
int main() {
    int cero = 0;
    cout << dividir(10, cero) << endl;
    return 0;
}
