// VULN: doble liberacion (double free)
// STATIC: Doble liberación
// DYNAMIC: heap | doble liberacion
#include <iostream>
using namespace std;
int main() {
    int* p = new int;
    *p = 7;
    delete p;
    delete p;
    cout << "fin" << endl;
    return 0;
}
