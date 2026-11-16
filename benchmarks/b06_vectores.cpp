// Benchmark: objetos (constructores, destructores, metodos, copia por valor)
#include <iostream>
using namespace std;
class Vec {
public:
    int x;
    int y;
    Vec() : x(0), y(0) {}
    Vec(int a, int b) : x(a), y(b) {}
    Vec(const Vec& o) : x(o.x), y(o.y) {}
    ~Vec() {}
    int dot(const Vec& o) const { return x * o.x + y * o.y; }
    void add(const Vec& o) { x = x + o.x; y = y + o.y; }
};
int combinar(Vec a, Vec b) {
    return a.dot(b) % 1000;
}
int main() {
    Vec acc(1, 2);
    int total = 0;
    for (int i = 0; i < 20000000; i++) {
        Vec t(i % 10, (i + 3) % 7);
        acc.add(t);
        acc.x = acc.x % 1000;
        acc.y = acc.y % 1000;
        total = (total + combinar(acc, t)) % 100003;
    }
    cout << total << " " << acc.x << " " << acc.y << endl;
    return 0;
}
