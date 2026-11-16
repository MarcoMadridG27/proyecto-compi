// VULN: uso despues de liberar (use-after-free)
// STATIC: Uso después de liberar
// DYNAMIC: heap | uso despues de liberar
#include <iostream>
using namespace std;
int main() {
    int* p = new int;
    *p = 42;
    delete p;
    cout << *p << endl;
    return 0;
}
