// VULN: recursion sin caso base (agota la pila)
// DYNAMIC: recursion | limite de profundidad de recursion
#include <iostream>
using namespace std;
int bajar(int n) {
    return bajar(n + 1) + 1;
}
int main() {
    cout << bajar(0) << endl;
    return 0;
}
