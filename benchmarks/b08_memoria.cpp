// Benchmark: memoria dinamica (new/delete en bucle, acceso por puntero, string)
#include <iostream>
#include <string>
using namespace std;
int main() {
    int total = 0;
    for (int i = 0; i < 3000000; i++) {
        int* p = new int[16];
        for (int j = 0; j < 16; j++) p[j] = i + j;
        for (int j = 0; j < 16; j++) total = (total + p[j]) % 100003;
        delete[] p;
    }
    string s = "";
    for (int i = 0; i < 200000; i++) s.push_back('a' + i % 26);
    int h = 0;
    for (int i = 0; i < 200000; i++) h = (h * 31 + s[i]) % 1000003;
    cout << total << " " << h << " " << s.size() << endl;
    return 0;
}
