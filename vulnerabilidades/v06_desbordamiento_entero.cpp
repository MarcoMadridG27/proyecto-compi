// VULN: el factorial desborda un int con signo
// DYNAMIC: overflow | desbordamiento aritmetico de entero
#include <iostream>
using namespace std;
int main() {
    int f = 1;
    for (int i = 1; i <= 20; i++) {
        f = f * i;
    }
    cout << f << endl;
    return 0;
}
