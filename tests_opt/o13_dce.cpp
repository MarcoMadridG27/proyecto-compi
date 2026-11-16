// OPT-ASM-ABSENT: nunca
#include <iostream>
using namespace std;
int g(int x) {
    if (x > 0) {
        return 1;
        cout << "nunca" << endl;      // inalcanzable
    }
    return 0;
    cout << "nunca" << endl;          // inalcanzable
}
int main() {
    const int DEBUG = 0;
    if (DEBUG) cout << "nunca depuracion" << endl;
    if (1) cout << "rama then" << endl; else cout << "nunca else" << endl;
    if (0) cout << "nunca then" << endl; else cout << "rama else" << endl;
    while (0) { cout << "nunca while" << endl; }
    int i = 0;
    while (true) {
        i++;
        if (i > 3) break;
        continue;
        cout << "nunca tras continue" << endl;
    }
    cout << g(5) << g(-1) << i << endl;
    return 0;
}
