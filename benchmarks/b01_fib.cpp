// Benchmark: recursion (llamadas a funcion, prologo/epilogo, pila)
#include <iostream>
using namespace std;
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
int main() {
    cout << fib(38) << endl;
    return 0;
}
