// OPT-ASM-ABSENT: cmpl $0, %eax
#include <iostream>
using namespace std;
const int LIM = 6;
int main() {
    int n = 10;            // nunca se modifica: se propaga
    int m = 3;
    int k = 5;
    bool flag = true;
    char c = 'a';
    int acc = 0;
    for (int i = 0; i < n; i++) acc = acc + i * m;
    k = k + 1;             // k se modifica: no se propaga
    int r = 7;
    int* pr = &r;          // se toma su direccion: no se propaga
    *pr = 9;
    int s = 4;
    cin >> s;              // cin lo modifica: no se propaga
    cout << acc << " " << k << " " << r << " " << s << " " << flag << c << LIM << endl;
    return 0;
}
