// Benchmark: producto de matrices de double (arreglos 2D, punto flotante SSE)
#include <iostream>
using namespace std;
const int N = 320;
double A[N][N];
double B[N][N];
double C[N][N];
int main() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = (i + j) % 7 + 0.5;
            B[i][j] = (i * j) % 5 + 1.0;
            C[i][j] = 0.0;
        }
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double s = 0.0;
            for (int k = 0; k < N; k++) {
                s = s + A[i][k] * B[k][j];
            }
            C[i][j] = s;
        }
    }
    double total = 0.0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) total = total + C[i][j];
    }
    cout << total << endl;
    return 0;
}
