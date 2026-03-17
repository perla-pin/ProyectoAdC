// =========================================================================
// SIMULADOR CPU x86 - LISTA EN MEMORIA SIMULADA
// Proyecto: Arquitectura de Computadoras
// Modelo Von Neumann - Arquitectura Intel x86
// Equipo: Carlos Ivan Becerra Quintero - 25110102
// =========================================================================
//
// COMO USAR EL PROGRAMA:
//   Escriba un dato  + ENTER  -> queda como dato pendiente
//   Escriba '@'     + ENTER  -> guarda el dato pendiente en la lista
//   Escriba un numero + ENTER -> queda como dato pendiente
//   Escriba '#'     + ENTER  -> lee lista[numero pendiente]
//   Escriba '$'     + ENTER  -> muestra toda la lista
//   Escriba '!'     + ENTER  -> termina el programa
//
// MAPA DE MEMORIA RAM (65536 posiciones, 1 byte c/u):
//   0x0000 - 0x000F : Reservado (EBP = 0 siempre)
//   0x0010 - 0x010F : CHAR MAP  - tabla ASCII 0-255 hardcodeada
//   0x0110 - 0x020F : BUFFER    - entrada actual del teclado
//   0x0210 - 0x030F : PENDING   - ultimo dato esperando comando
//   0x0310 - 0x7FFF : LIST      - zona de la lista de datos
//   0x8000 - ...    : Programa principal (main)
//   0xA000 - ...    : INT_TECLADO  - leer entrada del usuario
//   0xB000 - ...    : INT_MEMCPY   - copiar bloque de memoria
//   0xC000 - ...    : INT_PRINT    - mostrar elemento via MAR/MBR
//   0xFFFF          : Tope del stack
//
// INTERRUPCIONES:
//   0xA000 INT_TECLADO : Lee cadena del teclado -> deposita en BUFFER via MAR/MBR
//   0xB000 INT_MEMCPY  : Copia ELEM_SIZE bytes de ESI a EDI via MAR/MBR
//   0xC000 INT_PRINT   : Imprime cadena en RAM[EAX] char por char via MAR/MBR
//
// CICLO DE INSTRUCCION (Von Neumann):
//   Fetch  : int eip_actual = EIP   (UC lee direccion actual)
//   Decode : switch(EIP)            (UC identifica la instruccion)
//   Execute: case correspondiente   (ALU/UC ejecuta la microoperacion)
//
// ACCESO A MEMORIA: Todo via MAR y MBR sin excepcion.
//   Lectura : MAR=addr -> MBR=RAM[MAR] -> dest=MBR
//   Escritura: MAR=addr -> MBR=valor   -> RAM[MAR]=MBR
// =========================================================================

#include <iostream>
#include <cstdio>
#include <cstring>
using namespace std;

// =========================================================================
// ZONAS DE MEMORIA
// =========================================================================
#define CHAR_MAP_BASE 0x0010
#define BUFFER_BASE 0x0110
#define PENDING_BASE 0x0210
#define LIST_BASE 0x0310
#define ELEM_SIZE 0x0010 // 16 bytes por elemento

// Codigos ASCII de los comandos
#define CMD_SAVE 0x40 // '@'
#define CMD_READ 0x23 // '#'
#define CMD_EXIT 0x21 // '!'
#define CMD_SHOW 0x24 // '$'

// =========================================================================
// 1. ARQUITECTURA (CPU + RAM)
// =========================================================================
int RAM[65536] = {0};

// Registros de proposito general
int EAX = 0; // Accumulator: resultados, datos temporales, puntero de lectura
int EBX = 0; // Base: puntero a la siguiente celda libre de la lista
int ECX = 0; // Counter: contador de bucles y conversion numerica
int EDX = 0; // Data: contador de elementos guardados en la lista

// Registros de pila y flujo
int ESP = 0; // Stack Pointer: tope de la pila (crece hacia abajo)
int EBP = 0; // Base Pointer: ancla del marco de funcion
int EIP = 0; // Instruction Pointer: direccion de instruccion actual

// Registros de segmento para INT_MEMCPY
int ESI = 0; // Source Index: direccion fuente para la copia
int EDI = 0; // Destination Index: direccion destino para la copia

// Registros internos de memoria (Von Neumann)
int MAR = 0; // Memory Address Register
int MBR = 0; // Memory Buffer Register

// Bandera
int ZF = 0; // Zero Flag: 1 si resultado fue cero

bool HLT_flag = false;
int ciclo = 1;

// =========================================================================
// VISUALIZACION
// =========================================================================
void imprimir_encabezado()
{
    printf("------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("CICLO | EIP    | INSTRUCCION            | EAX  | EBX  | ECX  | EDX  | ESI  | EDI  | EBP  | ESP  | MAR  | MBR  | ZF | TOPE\n");
    printf("------------------------------------------------------------------------------------------------------------------------------------\n");
}

void imprimir_fila(int eip, const char *inst)
{
    int tope = RAM[ESP];
    printf("%04d  | 0x%04X | %-22s | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X | %04X |  %d | %d\n",
           ciclo, eip, inst,
           (unsigned short)EAX, (unsigned short)EBX,
           (unsigned short)ECX, (unsigned short)EDX,
           (unsigned short)ESI, (unsigned short)EDI,
           (unsigned short)EBP, (unsigned short)ESP,
           (unsigned short)MAR, (unsigned short)MBR,
           ZF, tope);
}

// =========================================================================
// CHAR MAP: hardcodea ASCII 0-255 en RAM antes de iniciar el programa.
// =========================================================================
void inicializar_char_map()
{
    for (int c = 0; c <= 255; c++)
        RAM[CHAR_MAP_BASE + c] = c;
}

// =========================================================================
// 2. HARDWARE ALU Y UNIDAD DE CONTROL (Microoperaciones)
// =========================================================================

// MOV: carga valor inmediato en registro.
void MOV(int &dest, int valor)
{
    dest = valor;
    EIP++;
}

// MOV_REG: copia registro a registro.
void MOV_REG(int &dest, int src)
{
    dest = src;
    EIP++;
}

// ADD: suma. ZF=1 si resultado==0.
void ADD(int &dest, int valor)
{
    dest += valor;
    ZF = (dest == 0) ? 1 : 0;
    EIP++;
}

// MUL: multiplica. En x86 MUL usa EAX implicito siempre.
void MUL(int &dest, int valor)
{
    dest *= valor;
    ZF = (dest == 0) ? 1 : 0;
    EIP++;
}

// DEC: decrementa. ZF=1 si resultado==0. DEC no modifica CF en x86 real.
void DEC(int &dest)
{
    dest--;
    ZF = (dest == 0) ? 1 : 0;
    EIP++;
}

// CMP: compara a y b. ZF=1 si iguales. No guarda resultado (igual que x86).
void CMP(int a, int b)
{
    ZF = (a == b) ? 1 : 0;
    EIP++;
}

// JE: salta si ZF==1 (Jump if Equal / Jump if Zero).
void JE(int addr) { EIP = (ZF == 1) ? addr : EIP + 1; }

// JNZ: salta si ZF==0 (Jump if Not Zero).
void JNZ(int addr) { EIP = (ZF == 0) ? addr : EIP + 1; }

// JMP: salto incondicional.
void JMP(int addr) { EIP = addr; }

// PUSH: ESP-=4, RAM[ESP]=valor via MAR/MBR.
void PUSH(int valor)
{
    ESP -= 4;
    MAR = ESP;
    MBR = valor;
    RAM[MAR] = MBR;
    EIP++;
}

// POP_VAL: MBR=RAM[ESP] via MAR/MBR, ESP+=4.
int POP_VAL()
{
    MAR = ESP;
    MBR = RAM[MAR];
    int v = MBR;
    ESP += 4;
    EIP++;
    return v;
}

// CALL: guarda dir. retorno en stack via MAR/MBR, salta al destino.
void CALL(int dest, int ret)
{
    ESP -= 4;
    MAR = ESP;
    MBR = ret;
    RAM[MAR] = MBR;
    EIP = dest;
}

// RET_OP: saca dir. retorno del stack via MAR/MBR -> EIP.
void RET_OP()
{
    MAR = ESP;
    MBR = RAM[MAR];
    EIP = MBR;
    ESP += 4;
}

// HLT_OP: detiene el ciclo de instruccion.
void HLT_OP() { HLT_flag = true; }

// =========================================================================
// 3. CICLO DE INSTRUCCION (Fetch - Decode - Execute)
// =========================================================================
int main()
{

    inicializar_char_map();

    EAX = 0;
    EBX = LIST_BASE;
    ECX = 0;
    EDX = 0;
    ESI = 0;
    EDI = 0;
    ESP = 0xFFFF;
    EBP = 0;
    EIP = 0x8000;
    MAR = 0;
    MBR = 0;
    ZF = 0;
    HLT_flag = false;
    ciclo = 1;

    printf("=== SIMULADOR CPU x86 - LISTA EN MEMORIA SIMULADA ===\n");
    printf("Char map : 0x0010-0x010F | Buffer  : 0x0110-0x020F\n");
    printf("Pending  : 0x0210-0x030F | Lista   : 0x0310-0x7FFF\n");
    printf("Programa : 0x8000        | INT_TECLADO : 0xA000\n");
    printf("INT_MEMCPY : 0xB000      | INT_PRINT   : 0xC000\n\n");
    printf("COMANDOS: '@'=guardar '#'=leer '$'=mostrar todo '!'=salir\n\n");
    imprimir_encabezado();

    while (!HLT_flag)
    {
        int eip_actual = EIP;

        switch (EIP)
        {

            // =================================================================
            // PROGRAMA PRINCIPAL --- 0x8000
            // Bucle principal: lee entrada, detecta comando o dato.
            // =================================================================

        case 0x8000:
            // Lee la entrada del usuario via INT_TECLADO.
            // CALL guarda 0x8001 en stack, salta a 0xA000.
            CALL(0xA000, 0x8001);
            imprimir_fila(eip_actual, "CALL 0xA000");
            break;

        case 0x8001:
            // Lee el primer char del BUFFER via MAR/MBR a EAX.
            // Determina si la entrada es un comando o un dato.
            MAR = BUFFER_BASE;
            MBR = RAM[MAR];
            EAX = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV EAX,[BUF]");
            break;

        case 0x8002:
            // Compara EAX con '@' (CMD_SAVE).
            CMP(EAX, CMD_SAVE);
            imprimir_fila(eip_actual, "CMP EAX,'@'");
            break;

        case 0x8003:
            // Si es '@' -> BLOQUE SAVE (0x8020).
            JE(0x8020);
            imprimir_fila(eip_actual, "JE 0x8020");
            break;

        case 0x8004:
            // Compara EAX con '#' (CMD_READ).
            CMP(EAX, CMD_READ);
            imprimir_fila(eip_actual, "CMP EAX,'#'");
            break;

        case 0x8005:
            // Si es '#' -> BLOQUE READ (0x8030).
            JE(0x8030);
            imprimir_fila(eip_actual, "JE 0x8030");
            break;

        case 0x8006:
            // Compara EAX con '$' (CMD_SHOW).
            CMP(EAX, CMD_SHOW);
            imprimir_fila(eip_actual, "CMP EAX,'$'");
            break;

        case 0x8007:
            // Si es '$' -> BLOQUE SHOW ALL (0x8040).
            JE(0x8040);
            imprimir_fila(eip_actual, "JE 0x8040");
            break;

        case 0x8008:
            // Compara EAX con '!' (CMD_EXIT).
            CMP(EAX, CMD_EXIT);
            imprimir_fila(eip_actual, "CMP EAX,'!'");
            break;

        case 0x8009:
            // Si es '!' -> BLOQUE EXIT (0x8060).
            JE(0x8060);
            imprimir_fila(eip_actual, "JE 0x8060");
            break;

        case 0x800A:
            // Es un DATO: copia BUFFER -> PENDING via INT_MEMCPY.
            // ESI = fuente (BUFFER_BASE), EDI = destino (PENDING_BASE).
            MOV(ESI, BUFFER_BASE);
            imprimir_fila(eip_actual, "MOV ESI,0x0110");
            break;

        case 0x800B:
            MOV(EDI, PENDING_BASE);
            imprimir_fila(eip_actual, "MOV EDI,0x0210");
            break;

        case 0x800C:
            // CALL INT_MEMCPY: copia ESI -> EDI (ELEM_SIZE bytes via MAR/MBR).
            CALL(0xB000, 0x800D);
            imprimir_fila(eip_actual, "CALL 0xB000");
            break;

        case 0x800D:
            // Regresa al inicio del bucle principal.
            JMP(0x8000);
            imprimir_fila(eip_actual, "JMP 0x8000");
            break;

            // =================================================================
            // BLOQUE SAVE --- 0x8020
            // Copia PENDING -> slot actual (EBX) via INT_MEMCPY.
            // Avanza EBX al siguiente slot libre e incrementa EDX.
            // =================================================================

        case 0x8020:
            // ESI = PENDING_BASE (fuente del dato a guardar).
            MOV(ESI, PENDING_BASE);
            imprimir_fila(eip_actual, "MOV ESI,0x0210");
            break;

        case 0x8021:
            // EDI = EBX (destino: siguiente celda libre de la lista).
            MOV_REG(EDI, EBX);
            imprimir_fila(eip_actual, "MOV EDI,EBX");
            break;

        case 0x8022:
            // CALL INT_MEMCPY: copia el dato pendiente al slot de la lista.
            CALL(0xB000, 0x8023);
            imprimir_fila(eip_actual, "CALL 0xB000");
            break;

        case 0x8023:
            // EBX += ELEM_SIZE: avanza al siguiente slot libre.
            ADD(EBX, ELEM_SIZE);
            imprimir_fila(eip_actual, "ADD EBX,0x10");
            break;

        case 0x8024:
            // EDX += 1: incrementa el contador de elementos guardados.
            ADD(EDX, 1);
            imprimir_fila(eip_actual, "ADD EDX,1");
            break;

        case 0x8025:
            // Limpia PENDING: escribe 0x00 en la primera posicion via MAR/MBR.
            MAR = PENDING_BASE;
            MBR = 0;
            RAM[MAR] = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV [PEND],0");
            break;

        case 0x8026:
            JMP(0x8000);
            imprimir_fila(eip_actual, "JMP 0x8000");
            break;

            // =================================================================
            // BLOQUE READ --- 0x8030
            // Convierte PENDING a indice entero via PARSE_NUM (loop inline).
            // Calcula la direccion: LIST_BASE + (indice * ELEM_SIZE).
            // Muestra el elemento via INT_PRINT.
            //
            // PARSE_NUM inline:
            //   ECX = 0 (acumulador del numero)
            //   Por cada char en PENDING:
            //     EAX = char - '0'  (digito)
            //     ECX = ECX * 10 + EAX
            // =================================================================

        case 0x8030:
            // Inicializa ECX = 0 (acumulador del numero).
            MOV(ECX, 0);
            imprimir_fila(eip_actual, "MOV ECX,0");
            break;

        case 0x8031:
            // MAR = PENDING_BASE, inicializa ESI como puntero de lectura.
            MOV(ESI, PENDING_BASE);
            imprimir_fila(eip_actual, "MOV ESI,0x0210");
            break;

        // PARSE_LOOP: etiqueta en 0x8032
        case 0x8032:
            // Lee el char actual de PENDING via MAR/MBR.
            MAR = ESI;
            MBR = RAM[MAR];
            EAX = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV EAX,[ESI]");
            break;

        case 0x8033:
            // Compara EAX con 0 (terminador). ZF=1 si fin de cadena.
            CMP(EAX, 0);
            imprimir_fila(eip_actual, "CMP EAX,0");
            break;

        case 0x8034:
            // Si terminador -> fin del PARSE_LOOP, salta a 0x8039.
            JE(0x8039);
            imprimir_fila(eip_actual, "JE 0x8039");
            break;

        case 0x8035:
            // EAX = EAX - '0': convierte char ASCII a digito entero.
            ADD(EAX, -(int)'0');
            imprimir_fila(eip_actual, "ADD EAX,-48");
            break;

        case 0x8036:
            // ECX = ECX * 10: desplaza el acumulador una posicion decimal.
            MUL(ECX, 10);
            imprimir_fila(eip_actual, "MUL ECX,10");
            break;

        case 0x8037:
            // ECX = ECX + EAX: agrega el digito actual al acumulador.
            ADD(ECX, EAX);
            imprimir_fila(eip_actual, "ADD ECX,EAX");
            break;

        case 0x8038:
            // ESI += 1: avanza al siguiente char de PENDING.
            ADD(ESI, 1);
            imprimir_fila(eip_actual, "ADD ESI,1");
            break;

        case 0x8038 + 1: // 0x8039
            // Fin del PARSE_LOOP. ECX = indice entero.
            // EAX = ELEM_SIZE * ECX = offset en bytes.
            MOV(EAX, ELEM_SIZE);
            imprimir_fila(eip_actual, "MOV EAX,0x10");
            break;

        case 0x803A:
            // EAX = EAX * ECX = offset en bytes desde LIST_BASE.
            // MUL en x86 usa EAX como operando implicito siempre.
            MUL(EAX, ECX);
            imprimir_fila(eip_actual, "MUL EAX,ECX");
            break;

        case 0x803B:
            // EAX = LIST_BASE + offset = direccion exacta del elemento.
            ADD(EAX, LIST_BASE);
            imprimir_fila(eip_actual, "ADD EAX,0x0310");
            break;

        case 0x803C:
            // CALL INT_PRINT: muestra el elemento en RAM[EAX] via MAR/MBR.
            CALL(0xC000, 0x803D);
            imprimir_fila(eip_actual, "CALL 0xC000");
            break;

        case 0x803D:
            JMP(0x8000);
            imprimir_fila(eip_actual, "JMP 0x8000");
            break;

            // =================================================================
            // BLOQUE SHOW ALL --- 0x8040
            // Muestra todos los elementos de la lista.
            // ECX = EDX (copia del contador para no destruir EDX).
            // EAX = puntero de lectura, avanza ELEM_SIZE por iteracion.
            // =================================================================

        case 0x8040:
            // ECX = EDX: contador del bucle de muestra.
            MOV_REG(ECX, EDX);
            imprimir_fila(eip_actual, "MOV ECX,EDX");
            break;

        case 0x8041:
            // EAX = LIST_BASE: puntero de lectura al inicio de la lista.
            MOV(EAX, LIST_BASE);
            imprimir_fila(eip_actual, "MOV EAX,0x0310");
            break;

        // SHOW_LOOP: etiqueta en 0x8042
        case 0x8042:
            // Compara ECX con 0. Si lista vacia o fin -> sale del bucle.
            CMP(ECX, 0);
            imprimir_fila(eip_actual, "CMP ECX,0");
            break;

        case 0x8043:
            // Si ECX == 0 -> fin del SHOW_LOOP, regresa al bucle principal.
            JE(0x8048);
            imprimir_fila(eip_actual, "JE 0x8048");
            break;

        case 0x8044:
            // CALL INT_PRINT: muestra el elemento actual en RAM[EAX].
            CALL(0xC000, 0x8045);
            imprimir_fila(eip_actual, "CALL 0xC000");
            break;

        case 0x8045:
            // EAX += ELEM_SIZE: avanza al siguiente elemento.
            ADD(EAX, ELEM_SIZE);
            imprimir_fila(eip_actual, "ADD EAX,0x10");
            break;

        case 0x8046:
            // ECX--: un elemento menos por mostrar.
            DEC(ECX);
            imprimir_fila(eip_actual, "DEC ECX");
            break;

        case 0x8047:
            // Si ZF==0 (quedan elementos): regresa a SHOW_LOOP.
            JNZ(0x8042);
            imprimir_fila(eip_actual, "JNZ 0x8042");
            break;

        case 0x8048:
            JMP(0x8000);
            imprimir_fila(eip_actual, "JMP 0x8000");
            break;

            // =================================================================
            // BLOQUE EXIT --- 0x8060
            // =================================================================

        case 0x8060:
            // HLT: detiene el ciclo de instruccion.
            HLT_OP();
            imprimir_fila(eip_actual, "HLT");
            break;

            // =================================================================
            // INT_TECLADO --- 0xA000
            // Simula la ISR del teclado (INT 21h en DOS / syscall read en Linux).
            // Proceso:
            //   1. Lee la cadena del usuario.
            //   2. Por cada char: MAR = CHAR_MAP_BASE + ascii,
            //                     MBR = RAM[MAR],
            //                     RAM[BUFFER_BASE + i] = MBR.
            //   3. Escribe terminador 0x00 al final.
            // Resultado: cadena en RAM[BUFFER_BASE] terminada en 0x00.
            // Convencion x86: prologo PUSH EBP / MOV EBP,ESP
            //                 epilogo MOV ESP,EBP / POP EBP / RET
            // =================================================================

        case 0xA000:
            // Prologo: guarda EBP del llamador en el stack via MAR/MBR.
            PUSH(EBP);
            imprimir_fila(eip_actual, "PUSH EBP");
            break;

        case 0xA001:
            // Prologo: establece nuevo marco de pila.
            MOV_REG(EBP, ESP);
            imprimir_fila(eip_actual, "MOV EBP,ESP");
            break;

        case 0xA002:
        {
            // INTERRUPCION: el teclado entrega la cadena al CPU.
            // Cada char es buscado en CHAR_MAP via MAR/MBR y
            // depositado en BUFFER_BASE.
            printf("--------------------------------------------------------------------------\n");
            printf(">>> INT TECLADO >>> ");
            fflush(stdout);
            char input[ELEM_SIZE + 1];
            memset(input, 0, sizeof(input));
            cin.getline(input, sizeof(input));
            int len = (int)strlen(input);
            if (len >= ELEM_SIZE)
                len = ELEM_SIZE - 1;
            for (int i = 0; i < len; i++)
            {
                unsigned char c = (unsigned char)input[i];
                MAR = CHAR_MAP_BASE + c;
                MBR = RAM[MAR];
                RAM[BUFFER_BASE + i] = MBR;
            }
            MAR = CHAR_MAP_BASE;
            MBR = RAM[MAR];
            RAM[BUFFER_BASE + len] = MBR;
            EIP++;
            imprimir_encabezado();
            imprimir_fila(eip_actual, "INT LEER_TECLADO");
            break;
        }

        case 0xA003:
            // Epilogo: restaura ESP.
            MOV_REG(ESP, EBP);
            imprimir_fila(eip_actual, "MOV ESP,EBP");
            break;

        case 0xA004:
            // Epilogo: restaura EBP del llamador.
            EBP = POP_VAL();
            imprimir_fila(eip_actual, "POP EBP");
            break;

        case 0xA005:
            // Epilogo: saca dir. retorno del stack -> EIP.
            RET_OP();
            imprimir_fila(eip_actual, "RET");
            break;

            // =================================================================
            // INT_MEMCPY --- 0xB000
            // Copia ELEM_SIZE bytes de RAM[ESI] a RAM[EDI] via MAR/MBR.
            // Simula REP MOVSB (Move String Byte con repeticion) de x86.
            // Registros: ESI = fuente, EDI = destino.
            // ECX se usa internamente como contador del loop de copia.
            // Se detiene antes si encuentra terminador 0x00.
            // Prologo/Epilogo estandar x86.
            // =================================================================

        case 0xB000:
            PUSH(EBP);
            imprimir_fila(eip_actual, "PUSH EBP");
            break;

        case 0xB001:
            MOV_REG(EBP, ESP);
            imprimir_fila(eip_actual, "MOV EBP,ESP");
            break;

        case 0xB002:
            // ECX = ELEM_SIZE: contador de bytes a copiar.
            MOV(ECX, ELEM_SIZE);
            imprimir_fila(eip_actual, "MOV ECX,ELEM_SZ");
            break;

        // MEMCPY_LOOP: etiqueta en 0xB003
        case 0xB003:
            // Lee byte fuente: MAR=ESI, MBR=RAM[MAR].
            MAR = ESI;
            MBR = RAM[MAR];
            EAX = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV EAX,[ESI]");
            break;

        case 0xB004:
            // Escribe byte destino: MAR=EDI, RAM[MAR]=EAX via MBR.
            MAR = EDI;
            MBR = EAX;
            RAM[MAR] = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV [EDI],EAX");
            break;

        case 0xB005:
            // Compara EAX con 0 (terminador). ZF=1 si fin de cadena.
            CMP(EAX, 0);
            imprimir_fila(eip_actual, "CMP EAX,0");
            break;

        case 0xB006:
            // Si terminador -> fin del MEMCPY_LOOP.
            JE(0xB00C);
            imprimir_fila(eip_actual, "JE 0xB00C");
            break;

        case 0xB007:
            // ESI += 1: avanza puntero fuente.
            ADD(ESI, 1);
            imprimir_fila(eip_actual, "ADD ESI,1");
            break;

        case 0xB008:
            // EDI += 1: avanza puntero destino.
            ADD(EDI, 1);
            imprimir_fila(eip_actual, "ADD EDI,1");
            break;

        case 0xB009:
            // DEC ECX: un byte menos por copiar.
            DEC(ECX);
            imprimir_fila(eip_actual, "DEC ECX");
            break;

        case 0xB00A:
            // Si ZF==0 (ECX no llego a 0): regresa a MEMCPY_LOOP.
            JNZ(0xB003);
            imprimir_fila(eip_actual, "JNZ 0xB003");
            break;

        case 0xB00B:
            // Garantiza terminador en la ultima posicion del slot destino.
            MAR = EDI;
            MBR = 0;
            RAM[MAR] = MBR;
            EIP++;
            imprimir_fila(eip_actual, "MOV [EDI],0");
            break;

        case 0xB00C:
            // Epilogo.
            MOV_REG(ESP, EBP);
            imprimir_fila(eip_actual, "MOV ESP,EBP");
            break;

        case 0xB00D:
            EBP = POP_VAL();
            imprimir_fila(eip_actual, "POP EBP");
            break;

        case 0xB00E:
            RET_OP();
            imprimir_fila(eip_actual, "RET");
            break;

            // =================================================================
            // INT_PRINT --- 0xC000
            // Muestra en consola la cadena en RAM[EAX] char por char via MAR/MBR.
            // Simula INT 21h funcion 09h (print string) de DOS,
            // o syscall write de Linux.
            // EAX = direccion base del elemento a imprimir.
            // Calcula el indice como: (EAX - LIST_BASE) / ELEM_SIZE.
            // Prologo/Epilogo estandar x86.
            // =================================================================

        case 0xC000:
            PUSH(EBP);
            imprimir_fila(eip_actual, "PUSH EBP");
            break;

        case 0xC001:
            MOV_REG(EBP, ESP);
            imprimir_fila(eip_actual, "MOV EBP,ESP");
            break;

        case 0xC002:
        {
            // Imprime el elemento en RAM[EAX] char a char via MAR/MBR.
            int idx = (EAX - LIST_BASE) / ELEM_SIZE;
            printf("  lista[%d] = \"", idx);
            for (int i = 0; i < ELEM_SIZE; i++)
            {
                MAR = EAX + i;
                MBR = RAM[MAR];
                if (MBR == 0)
                    break;
                printf("%c", (char)MBR);
            }
            printf("\"\n");
            EIP++;
            imprimir_fila(eip_actual, "INT PRINT_STR");
            break;
        }

        case 0xC003:
            MOV_REG(ESP, EBP);
            imprimir_fila(eip_actual, "MOV ESP,EBP");
            break;

        case 0xC004:
            EBP = POP_VAL();
            imprimir_fila(eip_actual, "POP EBP");
            break;

        case 0xC005:
            RET_OP();
            imprimir_fila(eip_actual, "RET");
            break;

        default:
            printf("ERROR: Violacion de segmento. EIP=0x%04X\n", EIP);
            HLT_flag = true;
            break;
        }

        ciclo++;
    }

    printf("--------------------------------------------------------------------------\n");
    printf("=== EJECUCION FINALIZADA ===\n");
    printf("Elementos en lista : %d\n", EDX);
    printf("Ciclos de reloj    : %d\n", ciclo - 1);
    system("pause");
    return 0;
}