// Benchmark: ordenamiento por burbuja (indexacion intensiva, comparaciones, intercambios)
#include <iostream>
using namespace std;
int datos[9000];
int main() {
    int n = 9000;
    int s = 12345;
    for (int i = 0; i < n; i++) {
        s = (s * 75 + 74) % 65537;
        datos[i] = s;
    }
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (datos[j] > datos[j + 1]) {
                int t = datos[j];
                datos[j] = datos[j + 1];
                datos[j + 1] = t;
            }
        }
    }
    int comprobacion = 0;
    for (int i = 0; i < n; i++) comprobacion = (comprobacion * 31 + datos[i]) % 1000003;
    cout << datos[0] << " " << datos[n - 1] << " " << comprobacion << endl;
    return 0;
}
