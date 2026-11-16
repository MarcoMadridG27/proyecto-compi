// VULN: conversion implicita con perdida de informacion
// STATIC: Conversión insegura de 'double' a 'int'
#include <iostream>
using namespace std;
int main() {
    double d = 3.99;
    int x = d;
    char c = 300;
    cout << x << endl;
    return 0;
}
