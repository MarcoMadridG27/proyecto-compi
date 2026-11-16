// OPT-ASM-CONTAINS: sall
#include <iostream>
using namespace std;
int f(int x) { return x * 8 + 3 * x; }
int main() {
    int a = 5;
    int b = a * 4;
    int c = 16 * (a + 1);
    int d = a * 3;
    int e = -a * 2;
    cout << b << " " << c << " " << d << " " << e << " " << f(7) << endl;
    return 0;
}
