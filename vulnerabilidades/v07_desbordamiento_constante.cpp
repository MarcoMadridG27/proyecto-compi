// VULN: desbordamiento en una expresion constante y division entre cero constante
// STATIC: Desbordamiento en expresión constante
// STATIC: División entre cero
#include <iostream>
using namespace std;
int main() {
    int a = 2147483647 + 1;
    int b = 10 / 0;
    cout << a << b << endl;
    return 0;
}
