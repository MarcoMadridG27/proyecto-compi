// Benchmark: integracion numerica de un oscilador (aritmetica de double en un bucle)
#include <iostream>
using namespace std;
int main() {
    double x = 1.0;
    double v = 0.0;
    double dt = 0.0001;
    double k = 4.0;
    for (int paso = 0; paso < 40000000; paso++) {
        double a = 0.0 - k * x;
        v = v + a * dt;
        x = x + v * dt;
    }
    cout << x << " " << v << endl;
    return 0;
}
