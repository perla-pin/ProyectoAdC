#include <iostream>
#include <cstring>
using namespace std;

char lista[256][16];
int total = 0;

void interrupcion_teclado(char *buffer)
{
    cin.getline(buffer, 16);
}

int main()
{
    char pendiente[16];
    char entrada[16];
    memset(pendiente, 0, sizeof(pendiente));

    cout << "COMANDOS: '@'=guardar  '#'=leer  '$'=mostrar todo  '!'=salir\n\n";

    while (true)
    {
        cout << ">>> ";
        interrupcion_teclado(entrada);

        if (strcmp(entrada, "@") == 0)
        {
            strcpy(lista[total], pendiente);
            total++;
            memset(pendiente, 0, sizeof(pendiente));
        }
        else if (strcmp(entrada, "#") == 0)
        {
            int idx = atoi(pendiente);
            cout << "lista[" << idx << "] = \"" << lista[idx] << "\"\n";
        }
        else if (strcmp(entrada, "$") == 0)
        {
            for (int i = 0; i < total; i++)
                cout << "lista[" << i << "] = \"" << lista[i] << "\"\n";
        }
        else if (strcmp(entrada, "!") == 0)
        {
            break;
        }
        else
        {
            strcpy(pendiente, entrada);
        }
    }

    system("pause");
    return 0;
}