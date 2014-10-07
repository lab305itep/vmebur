/*
    SvirLex 2012 - bur for VME with TSI148 chipset
    Using MEN drivers
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <MEN/vme4l.h>
#include <MEN/vme4l_api.h>
#include <readline/readline.h>

#ifndef SWAP_MODE
#define SWAP_MODE VME4L_NO_SWAP
#endif

#if (SWAP_MODE == VME4L_NO_SWAP)
#define SWAP(A) __swap(A)
#define SWAP2(A) __swap2(A)
#else
#define SWAP(A) (A)
#define SWAP2(A) (A)
#endif

#define ICXSPI		0x10010	// ICX SPI base address
#define ICXTMOUT	100	// ICX timeout counter
#define DACSPI		0x10020	// DAC SPI base address
#define DACTMOUT	100	// DAC timeout counter
#define I2CCLK		0x20000 // Local I2C (CLK control)
#define I2CTMOUT	1000	// I2C timeout counter
#define L2CCLK		0x10	// Remote I2C port
#define L2CTMOUT	100	// L2C operation timeout

typedef struct {
    unsigned addr;
    unsigned len;
    unsigned *ptr;
} VMEMAP;

void usleep(int num)
{
    struct timespec tm;
    tm.tv_sec = num / 1000000;
    tm.tv_nsec = (num % 1000000) * 1000;
    nanosleep(&tm, NULL);
}

unsigned __swap(unsigned i)
{
    union SWP {
	unsigned u;
	char c[4];
    } a, b;
    a.u = i;
    b.c[0] = a.c[3];
    b.c[1] = a.c[2];
    b.c[2] = a.c[1];
    b.c[3] = a.c[0];
    return b.u;
}

unsigned __swap2(unsigned short i)
{
    union SWP {
	unsigned short u;
	char c[2];
    } a, b;
    a.u = i;
    b.c[0] = a.c[1];
    b.c[1] = a.c[0];
    return b.u;
}

void Dump(unsigned addr, unsigned len, VMEMAP *map)
{
    unsigned i;
    if (addr + 4 > map->len) {
	printf("Nothing to print - start address (%8.8X) is above the mapped length (%8.8X)\n",
	    addr, map->len);
	return;
    }
    if (addr + len > map->len) len = map->len - addr;
    for (i = 0; i < len/4; i++) {
	if ((i%8) == 0) printf("%8.8X: ", map->addr + addr + 4*i);
	printf("%8.8X ", SWAP(map->ptr[addr/4+i]));
	if ((i%8) == 7) printf("\n");
    }
    if ((i%8) != 0) printf("\n");
}

void RegTest(int N, unsigned repeat, VMEMAP *map)
{
    int i;
    volatile unsigned *A;
    volatile unsigned *T;
    unsigned WA, RA, WT, RT;
    unsigned err;
    
    A = &(map->ptr[N*0x40 + 1]);
    T = &(map->ptr[N*0x40 + 3]);
    err = 0;
    srand48(time(0));
    
    for (i=0; i<repeat; i++) {
	WA = mrand48();
	WT = mrand48();
	*A = WA;
	*T = WT;
	RA = *A;
	RT = *T;
	if (WA != RA || WT != RT) {
	    err++;
	    if (err < 100) printf("%6.6X: W(%8.8X %8.8X) != R(%8.8X %8.8X)\n", i, WA, WT, RA, RT);
	}
    }
    printf("Test finished with %d errors.\n", err);
}

void MemTest(int N, unsigned addr, unsigned len, VMEMAP *map)
{
    int seed;
    int i;
    volatile unsigned *A;
    volatile unsigned *D;
    unsigned W, R, R1, R2, R3;
    unsigned err;
    
    A = &(map->ptr[N*0x40 + 1]);
    D = &(map->ptr[N*0x40 + 2]);
    seed = time(0);
    // Write
    *A = SWAP(addr);
    srand48(seed);
    for (i=0; i<len; i++) *D = mrand48();
    // Read
    err = 0;
    *A = SWAP(addr);
    srand48(seed);
    for (i=0; i<len; i++) {
	W = mrand48();
	R = *D;
	if (R != W) {
	    err++;
	    *A = SWAP(addr+i-1);
	    R1 = *D;
	    R2 = *D;
	    R3 = *D;
	    *A = SWAP(addr+i+1);
	    if (err < 100) 
		printf("%6.6X: R(%8.8X) != W(%8.8X) XOR=%8.8X [%8.8X, %8.8X, %8.8X]\n", 
		    i, R, W, R^W, R1, R2, R3);
	}
    }
    printf("ADC16 memory test finished with %d errors\n", err);
}

int I2CRead(VMEMAP *map, int addr)
{
    int i, data;
//	Chip address
    map->ptr[I2CCLK/4+3] = (addr >> 7) & 0xFE;	// no SWAP here and later in this function
    map->ptr[I2CCLK/4+4] = 0x90;		// Write | Start
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	Check NACK
    if (map->ptr[I2CCLK/4+4] & 0x80) {
	printf("I2C - NACK\n");
	return -1;
    }
//	Register address
    map->ptr[I2CCLK/4+3] = addr & 0xFF;
    map->ptr[I2CCLK/4+4] = 0x50;		// Write & Stop
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	Chip address
    map->ptr[I2CCLK/4+3] = ((addr >> 7) & 0xFE) | 1;
    map->ptr[I2CCLK/4+4] = 0x90;		// Write | Start
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	High byte
    map->ptr[I2CCLK/4+4] = 0x20;		// Read
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
    data = map->ptr[I2CCLK/4+3];
//	Low byte
    map->ptr[I2CCLK/4+4] = 0x68;		// Read | STOP | ACK
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
    data = (data << 8) | map->ptr[I2CCLK/4+3];

    return data;    
}

void I2CWrite(VMEMAP *map, int addr, int data)
{
    int i;
//	Chip address
    map->ptr[I2CCLK/4+3] = (addr >> 7) & 0xFE;	// no SWAP here and later in this function
    map->ptr[I2CCLK/4+4] = 0x90;		// Write | Start
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	Check NACK
    if (map->ptr[I2CCLK/4+4] & 0x80) {
	printf("I2C - NACK\n");
	return;
    }
//	Register address
    map->ptr[I2CCLK/4+3] = addr & 0xFF;
    map->ptr[I2CCLK/4+4] = 0x10;		// Write
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	Data high byte
    map->ptr[I2CCLK/4+3] = (data >> 8) & 0xFF;
    map->ptr[I2CCLK/4+4] = 0x10;		// Write
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;
//	Data low byte
    map->ptr[I2CCLK/4+3] = data & 0xFF;
    map->ptr[I2CCLK/4+4] = 0x50;		// Write & Stop
    for (i=0; i < I2CTMOUT; i++) if (!(map->ptr[I2CCLK/4+4] & 2)) break;    
}

int ICXRead(VMEMAP *map, int addr)
{
    int data;
    int i;
    
    map->ptr[ICXSPI/4 + 1] = SWAP(1);		// crystall select
    map->ptr[ICXSPI/4] = SWAP(0x80 | (addr >> 8));
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4] = SWAP(addr & 0xFF);
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4 + 1] = SWAP(0x101);	// input data
    map->ptr[ICXSPI/4] = 0;
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    data = SWAP(map->ptr[ICXSPI/4]);
    map->ptr[ICXSPI/4] = 0;
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    data = SWAP(map->ptr[ICXSPI/4]) | (data << 8);
    map->ptr[ICXSPI/4 + 1] = 0;			
    return data;
}

void ICXWrite(VMEMAP *map, int addr, int data)
{
    int i;
    
    map->ptr[ICXSPI/4 + 1] = SWAP(1);		// crystall select
    map->ptr[ICXSPI/4] = SWAP((addr >> 8) & 0x7F);
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4] = SWAP(addr & 0xFF);
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4] = SWAP(data >> 8);
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4] = SWAP(data & 0xFF);
    for (i=0; i<ICXTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[ICXSPI/4]))) break;
    map->ptr[ICXSPI/4 + 1] = 0;
}

int L2CRead(VMEMAP *map, int addr)
{
    int i, data;
    int l2cregs, chipadr, regadr;
    l2cregs = ((addr & 0x30000) >> 3) + L2CCLK;
    chipadr = (addr >> 7) & 0xFE;
    regadr = addr & 0xFF;
//	Chip address
    ICXWrite(map, l2cregs + 3, chipadr << 8);
    ICXWrite(map, l2cregs + 4, 0x9000);		// Write & Start
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	Check NACK
    if (ICXRead(map, l2cregs + 4) & 0x8000) {
	printf("I2C - NACK\n");
	return -1;
    }
//	Register address
    ICXWrite(map, l2cregs + 3, regadr << 8);
    ICXWrite(map, l2cregs + 4, 0x5000);		// Write | Stop
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	Chip address
    ICXWrite(map, l2cregs + 3, (chipadr | 1) << 8);
    ICXWrite(map, l2cregs + 4, 0x9000);		// Write | Start
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	High byte
    ICXWrite(map, l2cregs + 4, 0x9000);		// Read
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
    data = ICXRead(map, l2cregs + 3);
//	Low byte
    ICXWrite(map, l2cregs + 4, 0x6800);		// Read | STOP | ACK
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
    data = (data << 8) + ICXRead(map, l2cregs + 3);

    return data;    
}

void L2CWrite(VMEMAP *map, int addr, int data)
{
    int i;
    int l2cregs, chipadr, regadr;
    l2cregs = ((addr & 0x30000) >> 3) + L2CCLK;
    chipadr = (addr >> 7) & 0xFE;
    regadr = addr & 0xFF;
//	Chip address
    ICXWrite(map, l2cregs + 3, chipadr << 8);
    ICXWrite(map, l2cregs + 4, 0x9000);		// Write | Start
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	Check NACK
    if (ICXRead(map, l2cregs + 4) & 0x8000) {
	printf("I2C - NACK\n");
	return;
    }
//	Register address
    ICXWrite(map, l2cregs + 3, regadr << 8);
    ICXWrite(map, l2cregs + 4, 0x1000);		// Write
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	Data high byte
    ICXWrite(map, l2cregs + 3, data & 0xFF00);
    ICXWrite(map, l2cregs + 4, 0x1000);		// Write
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
//	Data low byte
    ICXWrite(map, l2cregs + 3, (data & 0xFF) << 8);
    ICXWrite(map, l2cregs + 4, 0x5000);		// Write | STOP
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
}

void DACWrite(VMEMAP *map, int data)
{
    int i;
    
    map->ptr[DACSPI/4 + 1] = SWAP(1);		// crystall select
    map->ptr[DACSPI/4] = SWAP((data >> 8) & 0xFF);
    for (i=0; i<DACTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[DACSPI/4]))) break;
    map->ptr[DACSPI/4] = SWAP(data & 0xFF);
    for (i=0; i<DACTMOUT; i++) if (!(0x8000 & SWAP(map->ptr[DACSPI/4]))) break;
    map->ptr[DACSPI/4 + 1] = 0;
}

void Help(void)
{
    int i;
    printf("\t ----- VME debug tool -----\n");
    printf("Command line: vmebur [options] [\"command(s)\"]\n");
    printf("\tOptions:\n");
    printf("-h - print this message\n");
    printf("-m{L|S|C} - data size: Long, Short, Char\n");
    printf("-q - quiet start\n");
    printf("-sNN - use space NN. Spaces:\n");
    for (i=0; i<30; i++) printf("%2.2d - %s%c", i, VME4L_SpaceName(i), ((i%5)==4) ? '\n' : '\t');
    printf("master6 is now mapped to A16D16 and master7 to CSR space.\n");
    printf("Command(s) should be enclosed in quotes and can be separated by semicolon.\n");
    printf("If no command is given - interactive mode is entered.\n");
    printf("\tCommands:\n");
    printf("* - nothing - just a comment;\n");
    printf("H - print this help message;\n");
    printf("I addr[=XX] - local I2C read/write;\n");
    printf("L xil&addr[=XX] - remoute I2C read/write;\n");
    printf("M [addr len] - map a region (query mapping);\n");
    printf("P [addr [len]] - dump the region. Address is counted from mapped start. 32-bit operations only.\n");
    printf("Q - quit;\n");
    printf("R N [repeat] - test read/write a pair of ADC16 registers (addr/trgicnt) for module N;\n");
    printf("S data - write data to local DAC;\n");
    printf("T N [addr [len]] - test ADC16 memory for module N;\n");
    printf("W [N] - wait N us, if no N - 1 ms;\n");
    printf("X addr[=XXXX] - interXilinx SPI read/write;\n");
    printf("AAAA[=XXXX] - read address AAAA / write XXXX to AAAA.\n");
    printf("Only the first letter of the command is decoded.\n");
    printf("ALL (!) input numbers are hexadecimal.\n");
}

int Map(unsigned addr, unsigned len, VMEMAP *map, int fd)
{
    int rc;
    if (map->ptr != NULL) VME4L_UnMap(fd, map->ptr, map->len);
    rc = VME4L_Map(fd, addr, len, (void **)&map->ptr);
    if (rc != 0) {
	map->addr = 0;
	map->len = 0;
	map->ptr = NULL;
	printf("Mapping region [%8.8X-%8.8X] failed with error %m\n",
	    addr, addr + len - 1);
    } else {
	map->addr = addr;
	map->len = len;
	printf("VME region [%8.8X-%8.8X] successfully mapped at %8.8X\n",
	    addr, addr + len - 1, map->ptr);
    }
    return rc;
}

int Process(char *cmd, int fd, VMEMAP *map, char mode)
{
    const char DELIM[] = " \t\n\r:=";
    char *tok;
    unsigned addr, len;
    int N;

    tok = strtok(cmd, DELIM);
    if (tok == NULL || strlen(tok) == 0) return 0;
    switch(toupper(tok[0])) {
    case '*':	// Comment
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	addr = strtoul(tok, NULL, 16);
	if (addr+4 > map->len) {
	    printf("Shift (%8.8X) above the mapped length (%8.8X)\n", addr, map->len);
	    break;
	}
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {	// read
	    switch (mode) {
	    case 'L':
	        printf("VME[%8.8X + %8.8X] = %8.8X\n", map->addr, addr, SWAP(map->ptr[addr/4]));
	        break;
	    case 'S':
	        printf("VME[%8.8X + %8.8X] = %4.4hX\n", map->addr, addr, SWAP2(((unsigned short *)map->ptr)[addr/2]) & 0xFFFF);
	        break;
	    case 'C':
	        printf("VME[%8.8X + %8.8X] = %2.2hhX\n", map->addr, addr, ((unsigned char *)map->ptr)[addr] & 0xFF);
	        break;
	    }
	} else {					// write
	    len = strtoul(tok, NULL, 16);
	    switch (mode) {
	    case 'L':
	        map->ptr[addr/4] = SWAP(len);
	        printf("VME[%8.8X + %8.8X] <= %8.8X\n", map->addr, addr, len);
	        break;
	    case 'S':
	        ((unsigned short *)map->ptr)[addr/2] = SWAP2(len) & 0xFFFF;
	        printf("VME[%8.8X + %8.8X] <= %4.4X\n", map->addr, addr, len);
	        break;
	    case 'C':
	        ((unsigned char *)map->ptr)[addr] = len & 0xFF;
	        printf("VME[%8.8X + %8.8X] <= %2.2X\n", map->addr, addr, len);
	        break;
	    }
	}
	break;
    case 'H':	// help
        Help();
        break;
    case 'I' :	// Local I2C read/write
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	if (I2CCLK+32 > map->len) {
	    printf("Local I2C registers (%8.8X) above the mapped length (%8.8X)\n", I2CCLK+32, map->len);
	    break;
	}
	addr = strtoul(tok, NULL, 16) & 0x7FFF;
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {	// read
	    printf("I2C[%4.4X] = %4.4X\n", addr, I2CRead(map, addr));
	} else {
	    N = strtol(tok, NULL, 16) & 0xFFFF;
	    I2CWrite(map, addr, N);
	    printf("I2C[%4.4X] <= %4.4X\n", addr, N);
	}
	break;
    case 'L' :	// Remote I2C read/write
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	if (ICXSPI+8 > map->len) {
	    printf("ICX SPI registers (%8.8X) above the mapped length (%8.8X)\n", ICXSPI+8, map->len);
	    break;
	}
	addr = strtoul(tok, NULL, 16) & 0x37FFF;
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {	// read
	    printf("L2C[%5.5X] = %2.2X\n", addr, L2CRead(map, addr));
	} else {
	    N = strtol(tok, NULL, 16) & 0xFF;
	    L2CWrite(map, addr, N);
	    printf("L2C[%5.5X] <= %2.2X\n", addr, N);
	}
	break;
    case 'M':	// Map address length
        tok = strtok(NULL, DELIM);
        if (tok == NULL || strlen(tok) == 0) {
    	    printf("VME region [%8.8X-%8.8X] is mapped at local address %8.8X\n",
    		map->addr, map->addr + map->len - 1, map->ptr);
	    break;
	}
	addr = strtoul(tok, NULL, 16);
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {
	    printf("Usage: Map address length\n");
	    break;
	}
	len = strtoul(tok, NULL, 16);
	Map(addr, len, map, fd);
	break;
    case 'P':	// Print [address [length]]
        if (map->ptr == NULL) {
	    printf("Map some region first.\n");
	    break;
	}
	addr = 0;
	len = map->len;
	tok = strtok(NULL, DELIM);
	if (tok != NULL && strlen(tok) != 0) {
	    addr = strtoul(tok, NULL, 16);
	    tok = strtok(NULL, DELIM);
	    if (tok != NULL && strlen(tok) != 0) len = strtoul(tok, NULL, 16);
	}
	Dump(addr, len, map);
	break;
    case 'Q' :	// Quit
        return 1;
    case 'R' :	// register read/write test
	if (map->ptr == NULL) {
	    printf("Map the region first. Most likely you need:\n\tM ADC16000 2000\n");
	    break;
	}
	len = 10000;	// repeat counter
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {
	    printf("Unit number is mandatory.\n");
	    break;
	}
	N = 0x1F & strtol(tok, NULL, 16);
	tok = strtok(NULL, DELIM);
	if (tok != NULL && strlen(tok) != 0) len = strtoul(tok, NULL, 16);
	RegTest(N, len, map);
	break;
    case 'S' :	// SPI to DAC
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	if (DACSPI+8 > map->len) {
	    printf("ICX SPI registers (%8.8X) above the mapped length (%8.8X)\n", DACSPI+8, map->len);
	    break;
	}
	N = strtoul(tok, NULL, 16) & 0xFFFF;
	DACWrite(map, N);
	printf("DAC <= %4.4X\n", N);
	break;
    case 'T' :	// test memory
        if (map->ptr == NULL) {
    	    printf("Map the region first. Most likely you need:\n\tM ADC16000 2000\n");
	    break;
	}
	addr = 0;
	len = 0x800000;	// 32 Mbytes = 8 Mdwords
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {
	    printf("Unit number is mandatory.\n");
	    break;
	}
	N = 0x1F & strtol(tok, NULL, 16);
	tok = strtok(NULL, DELIM);
	if (tok != NULL && strlen(tok) != 0) {
	    addr = strtoul(tok, NULL, 16);
	    tok = strtok(NULL, DELIM);
	    if (tok != NULL && strlen(tok) != 0) len = strtoul(tok, NULL, 16);
	}
	MemTest(N, addr, len, map);
	break;
    case 'W' :	// wait us
	tok = strtok(NULL, DELIM);
	N = (tok == NULL || strlen(tok) == 0) ? 1000 : strtol(tok, NULL, 16);
	usleep(N);
	break;
    case 'X' :	// InterXilinx read/write
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	if (ICXSPI+8 > map->len) {
	    printf("ICX SPI registers (%8.8X) above the mapped length (%8.8X)\n", ICXSPI+8, map->len);
	    break;
	}
	addr = strtoul(tok, NULL, 16) & 0x7FFF;
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {	// read
	    printf("ICX[%4.4X] = %4.4X\n", addr, ICXRead(map, addr));
	} else {
	    N = strtol(tok, NULL, 16) & 0xFFFF;
	    ICXWrite(map, addr, N);
	    printf("ICX[%4.4X] <= %4.4X\n", addr, N);
	}
	break;
    default:
	printf("Unknown command \"%c\"\n", toupper(tok[0]));
    }
    return 0;
}

int main(int argc, char **argv)
{
    int fd;
    int rc;
    char *cmd = NULL;
    char *command = NULL;
    char tok[256];
    char *ptr;
    VMEMAP map = {0, 0, NULL};
    char mode;	// Long - D32, Short - D16, Char - D8
    VME4L_SPACE spc, spcr;
    vmeaddr_t vmeaddr;
    int i, j;
    int quiet = 0;
    
    spc = VME4L_SPC_A32_D32;
    mode = 'L';
    for (i=1; i<argc; i++) {
	if (argv[i][0] == '-') switch (toupper(argv[i][1])) {
	case 'H':
	    Help();
	    goto Quit;
	case 'M':
	    mode = toupper(argv[i][2]);
	    if (mode != 'L' && mode != 'S' && mode != 'C') {
		printf("Unknown mode: %s\n", argv[i]);
		Help();
		goto Quit;
	    }
	    break;
	case 'Q':
	    quiet = 1;
	    break;
	case 'S':
	    spc = strtol(&argv[i][2], NULL, 0);
	    if (spc < 0 || spc >= 30) {
		printf("Unknown space: %s\n", argv[i]);
		Help();
		goto Quit;
	    }
	    break;
	default:
	    printf("Unknown option: %s\n", argv[i]);
	    Help();
	    goto Quit;
	} else {
	    command = argv[i];
	}
    }

    if (!quiet) 
	printf("\n\n\t\tManual VME controller: %s - %c\n\t\t\tSvirLex 2012\n\n", VME4L_SpaceName(spc), mode);
//	Open VME
    fd = VME4L_Open(spc);
    if (fd < 0) {
	printf("FATAL - can not open VME - %m\n");
	return fd;
    }
    rc = VME4L_SwapModeSet(fd, SWAP_MODE);
    if (rc) {
	printf("FATAL - can not set swap mode - %m\n");
	goto Quit;
    }

    if (command) {
	ptr = command;
	for (;ptr[0] != '\0';) {
	    for (j=0; j < sizeof(tok)-1; j++) {
	        if (ptr[j] == ';') {
	    	    tok[j] = '\0';
		    ptr += j+1;
		    break;
		} else if (ptr[j] == '\0') {
	    	    tok[j] = '\0';
		    ptr += j;
		    break;
		} else {
		    tok[j] = ptr[j];
		}
	    }
	    if (j < sizeof(tok)-1) {
		Process(tok, fd, &map, mode);
	    } else {
		printf("The single operation is too long: %s\n", ptr);
		break;
	    }
	}
    } else {
	for(;;) {
	    if (cmd) free(cmd);
	    cmd = readline("VmeBur (H-help)>");
	    if (cmd == NULL || strlen(cmd) == 0) continue;
	    add_history(cmd);
	    if (Process(cmd, fd, &map, mode)) break;
	    rc = VME4L_BusErrorGet(fd, &spcr, &vmeaddr, 1 );
	    if (rc) printf("VME BUS ERROR: rc=%d @ spc=%d addr=0x%X\n", rc, spcr, vmeaddr);
	}
    }
Quit:
    
//	Close VME	
    if (map.ptr != NULL) VME4L_UnMap(fd, map.ptr, map.len);
    VME4L_Close(fd);
    if (cmd) free(cmd);
    return 0;
}
