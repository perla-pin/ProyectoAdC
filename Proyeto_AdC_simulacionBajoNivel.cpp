// =========================================================================
// SIMULADOR CPU x86 - LISTA EN MEMORIA SIMULADA
// Proyecto: Arquitectura de Computadoras
// Modelo Von Neumann - Arquitectura Intel x86
// Equipo: Carlos Ivan Becerra Quintero - 25110102
// =========================================================================

#include <iostream>
#include <cstdio>
using namespace std;

// =========================================================================
// 1. INICIALIZACION DE LA ARQUITECTURA (CPU + RAM)
// =========================================================================

// Memoria RAM simulada (64KB = 65536 posiciones enteras)
int RAM[65536] = {0};

// Registros de la CPU (nomenclatura Intel x86)
int EAX = 0; // Acumulador: guarda resultados y datos temporales
int EBX = 0; // Base: puntero que recorre la lista en RAM
int ECX = 0; // Contador: controla los bucles de captura y muestra
int EDX = 0; // Datos: guarda N para recuperarlo y el indice de consulta
int ESP = 0; // Stack Pointer: tope de la pila (crece hacia abajo)
int EBP = 0; // Base Pointer: ancla del marco de la interrupcion
int EIP = 0; // Instruction Pointer: direccion de la instruccion actual
int ZF = 0;  // Zero Flag: se activa (=1) cuando DEC produce exactamente 0
bool HLT_flag = false;
int ciclo = 1;

// =========================================================================
// FUNCIONES DE VISUALIZACION
// =========================================================================

void imprimir_encabezado()
{
    printf("----------------------------------------------------------------------------------------------\n");
    printf("CICLO | EIP    | INSTRUCCION           | EAX  | EBX  | ECX  | EDX  | EBP  | ESP  | ZF | TOPE\n");
    printf("----------------------------------------------------------------------------------------------\n");
}

void imprimir_fila(int eip, const char *inst)
{
    int tope = RAM[ESP];
    printf("%04d  | 0x%04X | %-21s | %04X | %04X | %04X | %04X | %04X | %04X | %d  | %d\n",
           ciclo, eip, inst,
           (unsigned short)EAX,
           (unsigned short)EBX,
           (unsigned short)ECX,
           (unsigned short)EDX,
           (unsigned short)EBP,
           (unsigned short)ESP,
           ZF,
           tope);
}

// =========================================================================
// 2. HARDWARE DE LA ALU Y UNIDAD DE CONTROL (Microoperaciones)
// =========================================================================

// MOV: Carga un valor inmediato en un registro
void MOV(int &dest, int valor)
{
    dest = valor;
    EIP++;
}

// MOV_REG: Copia el valor de un registro a otro
void MOV_REG(int &dest, int src)
{
    dest = src;
    EIP++;
}

// MOV_MEM_WRITE: Escribe un valor en una direccion de la RAM
void MOV_MEM_WRITE(int addr, int valor)
{
    RAM[addr] = valor;
    EIP++;
}

// MOV_MEM_READ: Lee la RAM en la direccion dada y lo guarda en EAX
void MOV_MEM_READ(int addr)
{
    EAX = RAM[addr];
    EIP++;
}

// ADD: Suma un valor al destino
void ADD(int &dest, int valor)
{
    dest = dest + valor;
    EIP++;
}

// MUL: Multiplica EAX por el valor dado, resultado en EAX
// En x86 MUL usa EAX como operando implicito siempre
void MUL(int &dest, int valor)
{
    dest = dest * valor;
    EIP++;
}

// DEC: Decrementa un registro y actualiza ZF
// Simula el circuito de decremento de la ALU y las compuertas NOR
void DEC(int &dest)
{
    dest = dest - 1;
    ZF = (dest == 0) ? 1 : 0;
    EIP++;
}

// JNZ: Salta si ZF == 0 (resultado anterior no fue cero)
void JNZ(int addr)
{
    if (ZF == 0)
        EIP = addr;
    else
        EIP++;
}

// PUSH: Decrementa ESP en 4 y guarda el valor en el tope de la pila
void PUSH(int valor)
{
    ESP -= 4;
    RAM[ESP] = valor;
    EIP++;
}

// POP_VAL: Lee el tope de la pila y libera el espacio incrementando ESP en 4
int POP_VAL()
{
    int valor = RAM[ESP];
    ESP += 4;
    EIP++;
    return valor;
}

// CALL: Guarda la direccion de retorno en el stack y salta al destino
// Es un PUSH de la dir. de retorno seguido de un JMP
void CALL(int dest, int ret)
{
    ESP -= 4;
    RAM[ESP] = ret;
    EIP = dest;
}

// RET_OP: Saca la direccion de retorno del stack y la inyecta en EIP
void RET_OP()
{
    EIP = RAM[ESP];
    ESP += 4;
}

// HLT_OP: Detiene el ciclo de instruccion
void HLT_OP()
{
    HLT_flag = true;
}

// =========================================================================
// 3. CICLO DE INSTRUCCION (Fetch - Decode - Execute)
// =========================================================================
int main()
{

    EAX = 0;
    EBX = 0;
    ECX = 0;
    EDX = 0;
    ESP = 0xFFFF;
    EBP = 0;
    EIP = 0x0800;
    ZF = 0;
    HLT_flag = false;
    ciclo = 1;

    printf("=== SIMULADOR CPU x86 - LISTA EN MEMORIA SIMULADA ===\n");
    printf("Zona de lista en RAM : 0x0100 - 0x01FF\n");
    printf("Handler interrupcion : 0x0A00\n\n");
    imprimir_encabezado();

    while (!HLT_flag)
    {

        int eip_actual = EIP;

        switch (EIP)
        {

            // =============================================================
            // PROGRAMA PRINCIPAL (main) --- inicia en 0x0800
            // =============================================================

        case 0x0800:
            // Llama al handler para leer N (cuantos elementos ingresara el usuario)
            // CALL: guarda 0x0801 en el stack como dir. de retorno, salta a 0x0A00
            CALL(0x0A00, 0x0801);
            imprimir_fila(eip_actual, "CALL 0x0A00");
            break;

        case 0x0801:
            // ECX = EAX: el N leido se convierte en el contador del bucle de captura
            MOV_REG(ECX, EAX);
            imprimir_fila(eip_actual, "MOV ECX, EAX");
            break;

        case 0x0802:
            // EDX = EAX: guarda N en EDX para recuperarlo despues del bucle
            // ECX sera decrementado hasta 0, EDX conservara el valor original de N
            MOV_REG(EDX, EAX);
            imprimir_fila(eip_actual, "MOV EDX, EAX");
            break;

        case 0x0803:
            // EBX = 0x0100: apunta al inicio de la zona de lista en RAM
            MOV(EBX, 0x0100);
            imprimir_fila(eip_actual, "MOV EBX, 0x0100");
            break;

        // --- BUCLE DE CAPTURA (LOOP_STORE) --- etiqueta: 0x0804
        case 0x0804:
            // Llama al handler para leer el siguiente dato del usuario
            CALL(0x0A00, 0x0805);
            imprimir_fila(eip_actual, "CALL 0x0A00");
            break;

        case 0x0805:
            // RAM[EBX] = EAX: escribe el dato en la posicion actual de la lista
            MOV_MEM_WRITE(EBX, EAX);
            imprimir_fila(eip_actual, "MOV [EBX], EAX");
            break;

        case 0x0806:
            // EBX += 4: avanza al siguiente slot (cada entero ocupa 4 bytes en x86)
            ADD(EBX, 4);
            imprimir_fila(eip_actual, "ADD EBX, 4");
            break;

        case 0x0807:
            // ECX--: un elemento menos por capturar. ALU actualiza ZF
            DEC(ECX);
            imprimir_fila(eip_actual, "DEC ECX");
            break;

        case 0x0808:
            // Si ZF==0 (ECX no llego a 0): regresa a 0x0804 a leer otro dato
            // Si ZF==1 (ECX llego a 0): avanza a 0x0809 (mostrar lista)
            JNZ(0x0804);
            imprimir_fila(eip_actual, "JNZ 0x0804");
            break;

        // --- MOSTRAR LISTA COMPLETA (LOOP_SHOW) ---
        case 0x0809:
            // Reinicia EBX al inicio de la lista para recorrerla completa
            MOV(EBX, 0x0100);
            printf("--------------------------------------------------------------------------\n");
            printf(">>> LISTA ALMACENADA EN RAM:\n");
            imprimir_encabezado();
            imprimir_fila(eip_actual, "MOV EBX, 0x0100");
            break;

        case 0x080A:
            // ECX = EDX: recupera N para controlar el bucle de muestra
            MOV_REG(ECX, EDX);
            imprimir_fila(eip_actual, "MOV ECX, EDX");
            break;

        // --- LOOP_SHOW --- etiqueta: 0x080B
        case 0x080B:
        {
            // EAX = RAM[EBX]: carga el elemento actual de la lista
            int idx = (EBX - 0x0100) / 4;
            MOV_MEM_READ(EBX);
            imprimir_fila(eip_actual, "MOV EAX, [EBX]");
            printf("       >> lista[%d] = %d\n", idx, EAX);
            break;
        }

        case 0x080C:
            // EBX += 4: avanza al siguiente elemento
            ADD(EBX, 4);
            imprimir_fila(eip_actual, "ADD EBX, 4");
            break;

        case 0x080D:
            // ECX--: un elemento menos por mostrar
            DEC(ECX);
            imprimir_fila(eip_actual, "DEC ECX");
            break;

        case 0x080E:
            // Si ZF==0: regresa a 0x080B (quedan elementos)
            // Si ZF==1: avanza a 0x080F (consulta por indice)
            JNZ(0x080B);
            imprimir_fila(eip_actual, "JNZ 0x080B");
            break;

        // --- CONSULTA POR INDICE ---
        case 0x080F:
            // Imprime separador de seccion y llama al handler para leer el indice
            printf("--------------------------------------------------------------------------\n");
            printf(">>> CONSULTA POR INDICE:\n");
            imprimir_encabezado();
            CALL(0x0A00, 0x0810);
            imprimir_fila(eip_actual, "CALL 0x0A00");
            break;

        case 0x0810:
            // EDX = EAX: guarda el indice consultado en EDX
            MOV_REG(EDX, EAX);
            imprimir_fila(eip_actual, "MOV EDX, EAX");
            break;

        case 0x0811:
            // EBX = 0x0100: restaura la direccion base de la lista
            MOV(EBX, 0x0100);
            imprimir_fila(eip_actual, "MOV EBX, 0x0100");
            break;

        case 0x0812:
            // EAX = 4: carga el tamano de slot
            // Necesario porque MUL en x86 usa EAX como operando implicito
            MOV(EAX, 4);
            imprimir_fila(eip_actual, "MOV EAX, 4");
            break;

        case 0x0813:
            // EAX = EAX * EDX = 4 * indice = offset en bytes
            // MUL en x86 siempre multiplica contra EAX implicitamente
            MUL(EAX, EDX);
            imprimir_fila(eip_actual, "MUL EDX");
            break;

        case 0x0814:
            // EBX = base + offset: apunta exactamente al elemento pedido
            ADD(EBX, EAX);
            imprimir_fila(eip_actual, "ADD EBX, EAX");
            break;

        case 0x0815:
            // EAX = RAM[EBX]: carga el elemento consultado al acumulador
            MOV_MEM_READ(EBX);
            imprimir_fila(eip_actual, "MOV EAX, [EBX]");
            printf("       >> Consulta: lista[%d] = %d\n", EDX, EAX);
            break;

        case 0x0816:
            // Fin del programa
            HLT_OP();
            imprimir_fila(eip_actual, "HLT");
            break;

            // =============================================================
            // HANDLER DE INTERRUPCION --- INT_READ en 0x0A00
            // Simula que el periferico (teclado) entrega un dato a la CPU.
            // Convencion x86: el resultado de la funcion queda en EAX.
            // Sigue el prologo/epilogo estandar: PUSH EBP / MOV EBP,ESP
            //                                   ... / MOV ESP,EBP / POP EBP / RET
            // =============================================================

        case 0x0A00:
            // Prologo: guarda el EBP del llamador en el stack
            PUSH(EBP);
            imprimir_fila(eip_actual, "PUSH EBP");
            break;

        case 0x0A01:
            // Prologo: establece el nuevo marco de la funcion
            MOV_REG(EBP, ESP);
            imprimir_fila(eip_actual, "MOV EBP, ESP");
            break;

        case 0x0A02:
            // INTERRUPCION: el periferico (teclado) entrega el dato a EAX
            // Se corta la tabla para recibir entrada, luego se reimprimen headers
            printf("--------------------------------------------------------------------------\n");
            printf(">>> INTERRUPCION DE TECLADO >>> Ingrese un valor: ");
            cin >> EAX;
            EIP++;
            imprimir_encabezado();
            imprimir_fila(eip_actual, "INT LEER_TECLADO");
            break;

        case 0x0A03:
            // Epilogo: destruye variables locales restaurando ESP al marco base
            MOV_REG(ESP, EBP);
            imprimir_fila(eip_actual, "MOV ESP, EBP");
            break;

        case 0x0A04:
            // Epilogo: restaura el EBP del llamador desde el stack
            EBP = POP_VAL();
            imprimir_fila(eip_actual, "POP EBP");
            break;

        case 0x0A05:
            // Epilogo: saca la dir. de retorno del stack e inyecta en EIP
            RET_OP();
            imprimir_fila(eip_actual, "RET");
            break;

        default:
            printf("ERROR: Violacion de segmento. EIP=0x%04X fuera de control.\n", EIP);
            HLT_flag = true;
            break;
        }

        ciclo++;
    }

    printf("--------------------------------------------------------------------------\n");
    printf("=== EJECUCION FINALIZADA ===\n");
    printf("Resultado final en EAX = %d (0x%04X)\n", EAX, EAX);
    printf("--- APAGANDO SISTEMA ---\n\n");
    system("pause");
    return 0;
}