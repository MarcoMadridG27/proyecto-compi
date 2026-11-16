// OPT-ASM-CONTAINS: movl $100, %ecx
#include <iostream>
using namespace std;
int main() {
    int a = 1, b = 2, c = 3;
    int s = 0;
    for (int i = 0; i < 100; i++) {
        s = s + a * b + c;
        a = a + 1;
    }
    cout << s << endl;
    return 0;
}
