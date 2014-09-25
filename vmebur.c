/*
    SvirLex 2012 - bur for VME with TSI148 chipset
    Using MEN drivers
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
    unsigned addr;
    unsigned len;
    unsigned *ptr;
} VMEMAP;

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

void Help(void)
{
    printf("* - nothing - just a comment\n");
    printf("H - print this help message\n");
    printf("M [addr len] - map a region (query mapping).\n");
    printf("P [addr [len]] - dump the region. Address is counted from mapped start.\n");
    printf("Q|X - quit\n");
    printf("R N [repeat] - test read/write a pair of registers (addr/trgicnt) for module N.\n");
    printf("T N [addr [len]] - test ADC16 memory for module N.\n");
    printf("AAAA[=XXXX] - read address AAAA / write XXXX to AAAA\n");
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

int process(char *cmd, int fd, VMEMAP *map, char mode)
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
    case 'Q' :	// Quit / Exit
    case 'X' :
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
    VMEMAP map = {0, 0, NULL};
    char mode;	// Long - D32, Short - D16, Char - D8
    VME4L_SPACE spc, spcr;
    vmeaddr_t vmeaddr;
    
    spc = (argc > 1) ? strtol(argv[1], NULL, 0) : 8;
    mode = (argc > 2) ? toupper(argv[2][0]) : 'L';
    if (mode != 'L' && mode != 'S' && mode != 'C') mode = 'L';
    
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

    for(;;) {
	if (cmd) free(cmd);
	cmd = readline("VmeBur (H-help)>");
	if (cmd == NULL || strlen(cmd) == 0) continue;
	add_history(cmd);
	if (process(cmd, fd, &map, mode)) break;
	rc = VME4L_BusErrorGet(fd, &spcr, &vmeaddr, 1 );
	if (rc) printf("VME BUS ERROR: rc=%d @ spc=%d addr=0x%X\n", rc, spcr, vmeaddr);
    }
Quit:
    
//	Close VME	
    if (map.ptr != NULL) VME4L_UnMap(fd, map.ptr, map.len);
    VME4L_Close(fd);
    if (cmd) free(cmd);
    return 0;
}
