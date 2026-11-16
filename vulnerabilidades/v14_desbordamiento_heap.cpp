// VULN: escritura mas alla del final de un bloque de heap
// DYNAMIC: heap | desbordamiento de heap
#include <iostream>
using namespace std;
int main() {
    int* a = new int[4];
    for (int i = 0; i <= 4; i++) {
        a[i] = i;
    }
    delete[] a;
    cout << "fin" << endl;
    return 0;
}
