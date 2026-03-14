#include <iostream>
using namespace std;

int lista[256];

int interrupcion_teclado() {
    int dato;
    cin >> dato;
    return dato;
}

int main() {
    int n;
    cout << "Cuantos elementos? ";
    n = interrupcion_teclado();

    for (int i = 0; i < n; i++) {
        cout << "Dato [" << i + 1 << "]: ";
        lista[i] = interrupcion_teclado();
    }

    cout << "\nLista completa:\n";
    for (int i = 0; i < n; i++) {
        cout << "[" << i << "] = " << lista[i] << "\n";
    }

    int idx;
    cout << "\nQue indice consultar? ";
    idx = interrupcion_teclado();
    cout << "Valor en [" << idx << "] = " << lista[idx] << "\n";

    system("pause");

    return 0;
}