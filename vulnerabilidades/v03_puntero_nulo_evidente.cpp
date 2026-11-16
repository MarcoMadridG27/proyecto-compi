// VULN: desreferencia de un puntero que vale nullptr
// STATIC: vale nullptr
// DYNAMIC: null | desreferencia de puntero nulo
#include <iostream>
using namespace std;
int main() {
    int* p = nullptr;
    *p = 5;
    cout << "no deberia llegar aqui" << endl;
    return 0;
}
