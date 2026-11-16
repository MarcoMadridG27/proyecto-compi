// VULN: indice fuera de rango en un string
// DYNAMIC: bounds | acceso fuera de limites en arreglo
#include <iostream>
#include <string>
using namespace std;
int main() {
    string s = "abc";
    int i = 10;
    cout << s[i] << endl;
    return 0;
}
