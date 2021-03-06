/*
    SvirLex 2012 - bur for VME with TSI148 chipset
    Using vme_user drivers
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <readline/readline.h>

#include "vme_user.h"

#define SWAP(A) __swap(A)
#define SWAP2(A) __swap2(A)

#define ICXSPI		0x10010	// ICX SPI base address
#define ICXTMOUT	100	// ICX timeout counter
#define DACSPI		0x10020	// DAC SPI base address
#define DACTMOUT	100	// DAC timeout counter
#define I2CCLK		0x20000 // Local I2C (CLK control)
#define I2CTMOUT	1000	// I2C timeout counter
#define L2CCLK		0x10	// Remote I2C port
#define L2CTMOUT	100	// L2C operation timeout
#define SI5338ADR	0x70	// Si5338 I2C address
#define SITMOUT		100	// Si5338 timeout
#define ADCSPI		0	// ADC SPI controller registers base
#define MEMTEST		0x10100	// wb_tmem base addr
#define MEMSIZE		0x8000000	// DDR3 memory size in dwords

int quiet = 0;

typedef struct {
    unsigned addr;
    unsigned len;
    unsigned *ptr;
    void *mmap_ptr;
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

void GnuPlot(unsigned addr, unsigned len, VMEMAP *map)
{
    unsigned i;
    unsigned d;
    int d1, d2;
    if (addr + 4 > map->len) {
	printf("Nothing to dump - start address (%8.8X) is above the mapped length (%8.8X)\n",
	    addr, map->len);
	return;
    }
    if (addr + len > map->len) len = map->len - addr;
    for (i = 0; i < len/4; i++) {
	d = SWAP(map->ptr[addr/4+i]);
	d1 = d & 0xFFF;
	if (d1 & 0x800) d1 |= 0xFFFFF000;
	d2 = (d >> 16) & 0xFFF;
	if (d2 & 0x800) d2 |= 0xFFFFF000;
	printf("%d %d\n%d %d\n", 2*i, d1, 2*i+1, d2);
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

void MemTestRegs(VMEMAP *map, unsigned addr, unsigned len, int blen, int wp, int rp)
{
    int seed;
    unsigned i, ii, j, todo;
    volatile unsigned *wc;
    volatile unsigned *wd;
    volatile unsigned *rc;
    volatile unsigned *rd;
    unsigned W, R, R1, R2, R3;
    unsigned err;

    printf("Testing DDR3 memory %8.8X-%8.8X with bursts of %d through ports %d(W) %d(R)\n", addr, addr+len, blen, wp, rp);

    wc = &(map->ptr[MEMTEST/4 + wp*2]);
    wd = &(map->ptr[MEMTEST/4 + wp*2+1]);
    rc = &(map->ptr[MEMTEST/4 + 12 + rp*2]);
    rd = &(map->ptr[MEMTEST/4 + 12 + rp*2+1]);

    // always reset and wait
    *rd = 0;
    sleep(1);
    // check
    if (SWAP(*wc) != 0x801 || SWAP(*rc) != 0x1) {
	printf("Memory not initialized. Wport status: %8.8X Rport status %8.8X\n", SWAP(*wc), SWAP(*rc));
	return;
    }
    
    seed = time(0);

    // Write
    srand48(seed);
    for (i=addr, ii=addr; i<addr+len; i+= todo) {
	todo = blen;
	if (i+todo > addr+len) todo = addr+len-i;
	// load data fifo
	for (j=0; j<todo; j++) *wd = mrand48();
	// check status
	if (((SWAP(*wc) >> 4) & 0x7F) != todo) {
	    printf("\nError loading data FIFO at addr %8.8X: written %d words, status tells %d\n", i, todo, (SWAP(*wc) >> 4));
	    return;
	}
	// initiate write
	*wc = W = SWAP(i | ((todo-1)  << 27));
//printf("Wc = %8.8X\n", SWAP(W));
	// wait and check status
	for (j=0; j<5; j++) if ((R=SWAP(*wc)) == 0x801 ) break;
	if (j>= 5) {
	    printf("\nError executing write at addr %8.8X:  Wstatus %8.8X\n", i, R);
	    return;
	}
	if (i-ii > len/50) { 
	    printf("w"); 
	    fflush(stdout); 
	    ii = i;
	}
    }
    printf("\r");
    // Read
    err = 0;
    srand48(seed);
    for (i=addr, ii=addr; i<addr+len; i+= todo) {
	todo = blen;
	if (i+todo > addr+len) todo = addr+len-i;
	// initiate read
	*rc = W = SWAP(i | ((todo-1)  << 27));
//printf("Rc = %8.8X\n", SWAP(W));
	// wait and check status
	for (j=0; j<5; j++) if ((((R=SWAP(*rc)) >> 4) & 0x7F) == todo ) break;
	if (j >= 5) {
	    printf("\nError executing read at addr %8.8X:  Rstatus %8.8X\n", i, R);
	    return;
	}
	// read data fifo and compare
	for (j=0; j<todo; j++) {
	    if ((R=*rd) != (W=mrand48())) {
		printf("\nError at addr %8.8X: W:%8.8X R:%8.8X", i+j, W, R);
	        fflush(stdout); 
		err++;
	    }
	}
	// check status
	if (SWAP(*rc) != 0x1) {
	    printf("\nFIFO not empy after readingat addr %8.8X: status %8.8X\n", i, SWAP(*rc));
	    return;
	}
	if (i-ii > len/50) { 
	    printf("r"); 
	    fflush(stdout); 
	    ii = i;
	}
    }
    printf("\nDDR3 memory test finished with %d errors\n", err);
}


void MemTestWB(VMEMAP *map, unsigned addr, unsigned len)
{
    int seed;
    unsigned i, ii, j;
    volatile unsigned *ptr;
    unsigned W, R;
    unsigned err;
    unsigned leng;

    leng = len;
    if (addr + len > map->len) {
	leng = map->len - addr;
	printf("Can only test in the currently mapped window\n");
    }
    printf("Testing DDR3 memory %8.8X-%8.8X through VME A64 and WB RAM acess\n", addr, addr+leng);

    ptr = &(map->ptr[0]);

    // always reset and wait
    // not yet implemented
    //sleep(1);
    // check

    seed = time(0);

    // Write sequentially
    srand48(seed);
    
    for (i=addr/4, ii=addr/4; i<(addr+leng)/4; i++) {
	ptr[i] = mrand48();
	if (i-ii > leng/200) { 
	    printf("w"); 
	    fflush(stdout); 
	    ii = i;
	}
    }
    printf("\r");
    
    // Read sequentially
    err = 0;
    srand48(seed);
    for (i=addr/4, ii=addr/4; i<(addr+leng)/4; i++) {
	if ((R=ptr[i]) != (W=mrand48())) {
	    printf("\nError at addr %8.8X: W:%8.8X R:%8.8X", i*4, W, R);
	    fflush(stdout); 
	    err++;
	}
	if (i-ii > leng/200) { 
	    printf("r"); 
	    fflush(stdout); 
	    ii = i;
	}
    }
    printf("\nSequential DDR3 memory test finished with %d errors\n", err);

}


int I2CRead(VMEMAP *map, int addr)
{
    int i, data;
//	Chip address
    map->ptr[I2CCLK/4+3] = (addr >> 8) & 0xFE;	// no SWAP here and later in this function
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
    map->ptr[I2CCLK/4+3] = ((addr >> 8) & 0xFE) | 1;
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
    map->ptr[I2CCLK/4+3] = (addr >> 8) & 0xFE;	// no SWAP here and later in this function
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
//	Data byte
    ICXWrite(map, l2cregs + 4, 0x6800);		// Read | STOP | ACK
    for (i=0; i < I2CTMOUT; i++) if (!(ICXRead(map, l2cregs + 4) & 0x200)) break;
    data = (ICXRead(map, l2cregs + 3) >> 8) & 0xFF;

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
//	Data byte
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

void ADCWrite(VMEMAP *map, int N, int addr, int data)
{
    int spiregs;
    spiregs = ((N & 0xC) << 11) + ADCSPI;
    ICXWrite(map, spiregs+1, 1 << (N & 3));	// frame begin
    ICXWrite(map, spiregs, addr >> 8);
    ICXWrite(map, spiregs, addr & 0xFF);
    ICXWrite(map, spiregs, data & 0xFF);
    ICXWrite(map, spiregs+1, 0);		// frame end
}

int ADCRead(VMEMAP *map, int N, int addr)
{
    int data;
    int spiregs;
    spiregs = ((N & 0xC) << 11) + ADCSPI;

    ICXWrite(map, spiregs+1, 1 << (N & 3));	// frame begin
    ICXWrite(map, spiregs, (addr >> 8) | 0x80);
    ICXWrite(map, spiregs, addr & 0xFF);
    ICXWrite(map, spiregs+1, (1 << (N & 3)) + 0x100);	// switch to input data
    ICXWrite(map, spiregs, 0);
    ICXWrite(map, spiregs+1, 0);		// frame end
    data = ICXRead(map, spiregs);
    return data;
}

int SiRead(VMEMAP *map, int N, int addr)
{
    return L2CRead(map, (N << 16) | 0x7000 | addr);
}

void SiWrite(VMEMAP *map, int N, int addr, int data)
{
    L2CWrite(map, (N << 16) | 0x7000 | addr, data);
}

/*
    Old version: Configure Si5338 using register file and algorithm from fig. 9 of manual
    makes writes to all registers  instead of skipping some of them
*/
void ConfSI5338_old(VMEMAP *map, int N, char *fname)
{
    const unsigned char RegMask[512] = {
//		page 1
//	0     1     2     3     4     5     6     7     8     9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00,	//   0-  9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	//  10- 19
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF,	//  20- 29
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F,	//  30- 39
	0xFF, 0x7F, 0x3F, 0x00, 0x00, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF,	//  40- 49
	0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  50- 59
	0xFF, 0xFF, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  60- 69
	0xFF, 0xFF, 0xFF, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  70- 79
	0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 	//  80- 89
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0xFF, 0xFF, 0xFF, 	//  90- 99
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 	// 100-109
	0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 110-119
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 	// 120-129
	0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 130-139
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 140-149
	0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 	// 150-159
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 160-169
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 170-179
	0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 180-189
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 190-199
	0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 200-209
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 	// 210-219
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 	// 220-229
	0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 230-239
	0x00, 0xFF, 0x02, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 	// 240-249
	0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,                     	// 250-255
//		page 2	
//	0     1     2     3     4     5     6     7     8     9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	//   0-  9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	//  10- 19
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	//  20- 29
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	//  30- 39
	0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,	//  40- 49
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,	//  50- 59
	0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	//  60- 69
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0xFF,	//  70- 79
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	//  80- 89
	0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	//  90- 99
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 100-109
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 110-119
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 120-129
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 130-139
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 140-149
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 150-159
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 160-169
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 170-179
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 180-189
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 190-199
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 200-209
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 210-219
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 220-229
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 230-239
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 240-249
	0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,                     	// 250-255
    };
    unsigned char RegVal[512];
    char str[1024];
    char *tok;
    FILE *conf;
    const char DELIM[]=" \t\r:=,h";
    int i;
    unsigned char val;

//	Read the file
    conf = fopen(fname, "rt");
    if (!conf) {
	printf("SI5338 configuration file %s not found.\n", fname);
	return;
    }
    memset(RegVal, 0, sizeof(RegVal));
    for(;;) {
	if (!fgets(str, sizeof(str), conf)) break;
	tok = strtok(str, DELIM);
	if (!tok || !strlen(tok)) continue;
	if (!isdigit(tok[0])) continue;
	i = strtol(tok, NULL, 0);
	if (i<0 || i>511) continue;
	tok = strtok(NULL, DELIM);
	if (!tok || !strlen(tok)) continue;
	RegVal[i] = strtol(tok, NULL, 16);
    }
    fclose(conf);
    if (!quiet) printf("Configuring Si5338@block=%d with %s\n", N, fname);
//	Set OEB_ALL = 1; reg230[4]
    SiWrite(map, N, 230, 0x1F);		// disable all
//	Set DIS_LOL = 1; reg241[7]
    SiWrite(map, N, 241, 0xE5);
//	fire new configuration in
//	page 0
    SiWrite(map, N, 255, 0);
    for (i=0; i<255; i++) {
	switch(RegMask[i]) {
	case 0:			// we should ignore this register
	    break;
	case 0xFF:		// we can directly write to this register
	    SiWrite(map, N, i, RegVal[i]);
	    break;
	default:		// we need read-modify-write
	    val = SiRead(map, N, i) & (~RegMask[i]);
	    val |= RegVal[i] & RegMask[i];
	    SiWrite(map, N, i, val);
	    break;
	}
	val = SiRead(map, N, i) & RegMask[i];
	if (val != (RegVal[i] & RegMask[i])) {
	    printf("Register 0:%d write=%2.2X read=%2.2X mask=%2.2X\n", i, val, RegVal[i], RegMask[i]);
	}
    }
//	page 1
    SiWrite(map, N, 255, 1);
    for (i=0; i<255; i++) {
	switch(RegMask[256+i]) {
	case 0:			// we should ignore this register
	    break;
	case 0xFF:		// we can directly write to this register
	    SiWrite(map, N, i, RegVal[256+i]);
	    break;
	default:		// we need read-modify-write
	    val = SiRead(map, N, i) & (~RegMask[256+i]);
	    val |= RegVal[256+i] & RegMask[256+i];
	    SiWrite(map, N, i, val);
	    break;
	}
	val = SiRead(map, N, i) & RegMask[i+256];
	if (val != (RegVal[i+256] & RegMask[i+256])) {
	    printf("Register 1:%d write=%2.2X read=%2.2X mask=%2.2X\n", i, val, RegVal[i+256], RegMask[i+256]);
	}
    }
    SiWrite(map, N, 255, 0);	// back to page 0
//	Validate input clock
    for (i=0; i<SITMOUT; i++) {
	if (!(SiRead(map, N, 218) & 4)) break;
	usleep(100);
    }
    if (i == SITMOUT) {
	printf("SI5338 - can not validate input clock.\n");
	return;
    }
//	Set FCAL_OVRD_EN=0; reg49[7]
    val = SiRead(map, N, 49) & 0x7F;
    SiWrite(map, N, 49, val);
//	Initiate PLL lock SOFT_RESET=1; reg246[1]
    SiWrite(map, N, 246, 2);
    usleep(25000);
//	restart LOL DIS_LOL=0; reg241[7]; reg241 = 0x65
    SiWrite(map, N, 241, 0x65);
//	Validate PLL lock
    for (i=0; i<SITMOUT; i++) {
	if (!(SiRead(map, N, 218)& 0x15)) break;
	usleep(100);
    }
    if (i == SITMOUT) {
	printf("SI5338 - can not lock PLL.\n");
	return;
    }
//	Copy FCAL
    val = SiRead(map, N, 237) & 3;
    SiWrite(map, N, 47, val | 0x14);
    val = SiRead(map, N, 236);
    SiWrite(map, N, 46, val);
    val = SiRead(map, N, 235);
    SiWrite(map, N, 45, val);
//	Set FCAL_OVRD_EN=1; reg49[7]
    val = SiRead(map, N, 49) | 0x80;
    SiWrite(map, N, 49, val);
//	Enable Outputs
//	Set OEB_ALL = 0; reg230[4]
    SiWrite(map, N, 230, 0);		// enable all    
}

/*
    Configure Si5338 using register file and algorithm from fig. 9 of manual
    parsing .h file generated by Silabs ClockBuilder Desktop
    file structure must follow the pattaern:
	....
	#define name NREGS to program, only one line of 3 words starting with #
	....
	{ RegN, RegVal, RegMask }  exactly NREGS lines like this
	....
*/
void ConfSI5338(VMEMAP *map, int N, char *fname)
{
    unsigned char RegVal;
    unsigned char RegMask;
    int Reg, nRegs, page;
    char str[1024];
    char *tok;
    FILE *conf;
    const char DELIM[]=" \t\r,#{}";
    int i,j;
    unsigned char val;

//	Read the file
    conf = fopen(fname, "rt");
    if (!conf) {
	printf("SI5338 configuration file %s not found.\n", fname);
	return;
    }
// find #define
    nRegs = -1;
    for(i=0;;i++) {
	if (!fgets(str, sizeof(str), conf)) break;
	if (str[0] != '#') continue;
	tok = strtok(str, DELIM);	// "define"
	tok = strtok(NULL, DELIM);	// name
	tok = strtok(NULL, DELIM);	// NREGS
	if (!tok || !strlen(tok)) break;
	if (!isdigit(tok[0])) break;
	nRegs = strtol(tok, NULL, 0);
	break;
    }
    if (nRegs <= 0 || nRegs > 511) {
	printf("SI5338 configuration: wrong number of registers to configure %d, line %d\n", nRegs, i);
	fclose(conf);
	return;
    }
    if (!quiet) printf("Configuring Si5338@block=%d with %s\n", N, fname);
//	Set OEB_ALL = 1; reg230[4]
    SiWrite(map, N, 230, 0x1F);		// disable all
//	Set DIS_LOL = 1; reg241[7] (manual doesn't reqire to write 0x65)
    SiWrite(map, N, 241, 0x80);
//	select page 0 for safety
    SiWrite(map, N, 255, 0);
// pages are switched by register writes
    page = 0;
// parsing file and programming
    for(j=0;;i++) {
	if (!fgets(str, sizeof(str), conf)) break;
	if (str[0] != '{') continue;
    // parsing 3 numbers from lines starting with "{"
	tok = strtok(str, DELIM);
	if (!tok || !strlen(tok)) break;
	if (!isdigit(tok[0])) break;
	Reg = strtol(tok, NULL, 0);
	if (Reg<0 || Reg>511) break;
	tok = strtok(NULL, DELIM);
	if (!tok || !strlen(tok)) break;
	if (!isdigit(tok[0])) break;
	RegVal = strtol(tok, NULL, 0);
	tok = strtok(NULL, DELIM);
	if (!tok || !strlen(tok)) break;
	if (!isdigit(tok[0])) break;
	RegMask = strtol(tok, NULL, 0);
	if (Reg == 255) page = RegVal & 1;
    // programming
	switch(RegMask) {
	case 0:			// we should ignore this register
	    break;
	case 0xFF:		// we can directly write to this register
	    SiWrite(map, N, Reg, RegVal);
	    break;
	default:		// we need read-modify-write
	    val = SiRead(map, N, Reg) & (~RegMask);
	    val |= RegVal & RegMask;
	    SiWrite(map, N, Reg, val);
	    break;
	}
	val = SiRead(map, N, Reg);
	if ((val & RegMask) != (RegVal & RegMask)) {
	    printf("Page %d Register %d: write=%2.2X mask=%2.2X read=%2.2X \n", page, Reg, RegVal, RegMask, val);
	}
	j++;
    }
    fclose(conf);
    if (j != nRegs) {
	printf("SI5338 configuration: wrong format or number of progs %d doesn't match NREGS %d, line %d\n", j, nRegs, i);
	printf("!!! SI5338 configuration is incomplete !!!\n");
	return;
    }
    SiWrite(map, N, 255, 0);	// back to page 0 for safety
//	Validate input clock
    for (i=0; i<SITMOUT; i++) {
	if (!(SiRead(map, N, 218) & 4)) break;
	usleep(100);
    }
    if (i == SITMOUT) {
	printf("SI5338 - input clock is absent or invalid (LOS_CLKIN).\n");
	return;
    }
//	Set FCAL_OVRD_EN=0; reg49[7]
    val = SiRead(map, N, 49) & 0x7F;
    SiWrite(map, N, 49, val);
//	Initiate PLL lock SOFT_RESET=1; reg246[1]
    SiWrite(map, N, 246, 2);
    usleep(25000);
//	restart LOL DIS_LOL=0; reg241[7]; reg241 = 0x65
    SiWrite(map, N, 241, 0x65);
//	Validate PLL lock (no PLL_LOL, no LOS_CLKIN, no SYS_CAL)
    for (i=0; i<SITMOUT; i++) {
	if (!(SiRead(map, N, 218)& 0x15)) break;
	usleep(100);
    }
    if (i == SITMOUT) {
	printf("SI5338 - can not lock PLL.\n");
	return;
    }
//	Copy FCAL
    val = SiRead(map, N, 237) & 3;
    SiWrite(map, N, 47, val | 0x14);
    val = SiRead(map, N, 236);
    SiWrite(map, N, 46, val);
    val = SiRead(map, N, 235);
    SiWrite(map, N, 45, val);
//	Set FCAL_OVRD_EN=1; reg49[7]
    val = SiRead(map, N, 49) | 0x80;
    SiWrite(map, N, 49, val);
//	Enable Outputs
//	Set OEB_ALL = 0; reg230[4]
    SiWrite(map, N, 230, 0);		// enable all    
}


void GetEvents(VMEMAP *map, int addr, int N, char * tok)
{
    const struct {
	unsigned filled;
	unsigned data;
    } fifo[4] = {{0x10040, 0x40000}, {0x10050, 0x60000}, {0x10060, 0x80000}, {0x10070, 0xA0000}};
    FILE *f;
    unsigned int L, LL;
    unsigned int len;
    int i;
    volatile unsigned int *regfilled;
    volatile unsigned int *regfifo;
    unsigned int buf[0x2000];
    time_t tmr;
    
    f = fopen(((tok && strlen(tok) > 0) ? tok : "fifo.dat"), "wb");
    if (!f) {
	printf("Can not open file %s for writing.\n", tok);
	return;
    }
    
    regfilled = &(map->ptr[fifo[addr].filled/4]);
    regfifo = &(map->ptr[fifo[addr].data/4]);
    
    *regfifo = 0;		// reset FIFO
    
    LL = 0;
    tmr = time(NULL);
    for (L = 0;; L += len) {
	if (N >= 0) {
	    if (L > N) break;
	} else {
	    if (time(NULL) > tmr - N) break;
	}
	if (L - LL > 20000) {
	    printf(".");
	    fflush(stdout);
	    LL = L;
	}
	len = SWAP(*regfilled);
	if (len & 0x80000000) {		// overflow
	    printf("Fifo overflow\n");
	    *regfifo = 0;
	    len = 0;
	    continue;
	}
	if (!len) continue;
	if (len > sizeof(buf) / sizeof(int)) len = sizeof(buf) / sizeof(int);
	for (i=0; i<len; i++) buf[i] = SWAP(*regfifo);
	fwrite(buf, sizeof(int), len, f);
    }

    printf("\n");
    fclose(f);
}

void Help(void)
{
    int i;
    printf("\t ----- VME debug tool -----\n");
    printf("Command line: vmebur [options] [\"command(s)\"]\n");
    printf("\tOptions:\n");
    printf("-h - print this message\n");
    printf("-s{A16|A24|A32|A64|CRCSR} - address space\n");
    printf("-w{D8|D16|D32|D64} - data size\n");
    printf("-q - quiet start\n");
    printf("Command(s) should be enclosed in quotes and can be separated by semicolon.\n");
    printf("If no command is given - interactive mode is entered.\n");
    printf("\tCommands:\n");
    printf("* - nothing - just a comment;\n");
    printf("G N addr[=XX] - read/write register addr @ ADC N via SPI;\n");
    printf("H - print this help message;\n");
    printf("I addr[=XX] - local I2C read/write;\n");
    printf("J [addr [len]] - dump data for gnuplot;\n");
    printf("K N clkregfile.txt - load SI5338 configuration file to 16-chan block N;\n");
    printf("L xil&addr[=XX] - remoute I2C read/write;\n");
    printf("M [addr len] - map a region (query mapping);\n");
    printf("N xil num[s] [file.dat] - get num 32 bit words  from FIFO (or run for num seconds) for xil to file;\n");
    printf("P [addr [len]] - dump the region. Address is counted from mapped start. 32-bit operations only.\n");
    printf("Q - quit;\n");
    printf("R N [repeat] - test read/write a pair of ADC16 registers (addr/trgicnt) for module N;\n");
    printf("S data [data2] - write data and optionally data2 to local DAC;\n");
    printf("T [addr [len [burstlen [wport rport]]]] - test wfd125 DDR3 memory up to 128M 32-bit words\n");
    printf("W [N] - wait N us, if no N - 1 ms;\n");
    printf("X addr[=XXXX] - interXilinx SPI read/write;\n");
    printf("AAAA[=XXXX] - read address AAAA / write XXXX to AAAA.\n");
    printf("Only the first letter of the command is decoded.\n");
    printf("ALL (!) input numbers are hexadecimal. Commands are case insensitive\n");
}

unsigned long long GetMaxAddr(unsigned aspace)
{
    switch(aspace)
    {
        case VME_A16: return VME_A16_MAX;
	case VME_A24: return VME_A24_MAX;
	case VME_A32: return VME_A32_MAX;
//	case VME_A64: return VME_A64_MAX;
	case VME_CRCSR: return VME_CRCSR_MAX;
	default: return -1;
    }
}

int Map(unsigned addr, unsigned len, VMEMAP *map, int fd, struct vme_master *master)
{
    int rc;
    unsigned offset;
    if (map->mmap_ptr != NULL) munmap(map->mmap_ptr, map->len);

    offset = (addr & 0xFFFF);
    // We first adjust the window
    master->vme_addr = addr - offset;
    master->size = len + offset;
    // Workaround for "Invalid PCI bound alignment"
    if (master->size & 0xFFFF)
    {
	printf("Fix master size from 0x%llx ", master->size);
        master->size += 0x10000 - (master->size & 0xFFFF);
	printf("to 0x%llx\n", master->size);
    }

    if (master->vme_addr + master->size > GetMaxAddr(master->aspace))
    {
        master->size = GetMaxAddr(master->aspace) - master->vme_addr;
    }

    rc = ioctl(fd, VME_SET_MASTER, master);
    if (rc != 0) {
	printf("vme_addr = 0x%llx\n", master->vme_addr);
	printf("size = 0x%llx\n", master->size);
        perror("W125C: FATAL - can not setup VME window");
        return rc;
    }

    // Now VME addr 'addr' will be at 0 offset for this fd.
    map->mmap_ptr = mmap(NULL, master->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map->mmap_ptr == MAP_FAILED)
    {
	printf("Mapping region [%8.8X-%8.8X] failed with error %m\n",
	    addr, addr + len - 1);
	map->addr = 0;
	map->len = 0;
	map->ptr = NULL;
	map->mmap_ptr = NULL;
    } else {
	map->addr = addr;
	map->len = len;
        map->ptr = (unsigned*)((char*)map->mmap_ptr + offset);
	if (!quiet) printf("# VME region [%8.8X-%8.8X] successfully mapped at %p\n",
	    addr, addr + len - 1, map->ptr);
        return 0;
    }
    return 1;
}

int Process(char *cmd, int fd, VMEMAP *map, struct vme_master *master)
{
    const char DELIM[] = " \t\n\r:=";
    char *tok;
    unsigned addr, len, blen, wp, rp;
    int N, M;

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
	    switch (master->dwidth) {
	    case VME_D32:
	        printf("VME[%8.8X + %8.8X] = %8.8X\n", map->addr, addr, SWAP(map->ptr[addr/4]));
	        break;
	    case VME_D16:
	        printf("VME[%8.8X + %8.8X] = %4.4hX\n", map->addr, addr, SWAP2(((unsigned short *)map->ptr)[addr/2]) & 0xFFFF);
	        break;
	    case VME_D8:
	        printf("VME[%8.8X + %8.8X] = %2.2hhX\n", map->addr, addr, ((unsigned char *)map->ptr)[addr] & 0xFF);
	        break;
	    }
	} else {					// write
	    len = strtoul(tok, NULL, 16);
	    switch (master->dwidth) {
	    case VME_D32:
	        map->ptr[addr/4] = SWAP(len);
	        if (!quiet) printf("VME[%8.8X + %8.8X] <= %8.8X\n", map->addr, addr, len);
	        break;
	    case VME_D16:
	        ((unsigned short *)map->ptr)[addr/2] = SWAP2(len) & 0xFFFF;
	        if (!quiet) printf("VME[%8.8X + %8.8X] <= %4.4X\n", map->addr, addr, len);
	        break;
	    case VME_D8:
	        ((unsigned char *)map->ptr)[addr] = len & 0xFF;
	        if (!quiet) printf("VME[%8.8X + %8.8X] <= %2.2X\n", map->addr, addr, len);
	        break;
	    }
	}
	break;
    case 'G' :	// Remote SPI read/write
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
	N = strtoul(tok, NULL, 16) & 0xF;
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
	addr = strtoul(tok, NULL, 16) & 0x1FFF;
	tok = strtok(NULL, DELIM);
	if (tok == NULL || strlen(tok) == 0) {	// read
	    printf("ADC[%1.1X:%4.4X] = %2.2X\n", N, addr, ADCRead(map, N, addr));
	} else {
	    len = strtol(tok, NULL, 16) & 0xFF;
	    ADCWrite(map, N, addr, len);
	    if (!quiet) printf("ADC[%1.1X:%4.4X] <= %2.2X\n", N, addr, len);
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
	    if (!quiet) printf("I2C[%4.4X] <= %4.4X\n", addr, N);
	}
	break;
    case 'J':	// Dump [address [length]] for gnuplot
        if (map->ptr == NULL) {
	    printf("Map some region first.\n");
	    break;
	}
	addr = 0;
	len = 0x200;
	tok = strtok(NULL, DELIM);
	if (tok != NULL && strlen(tok) != 0) {
	    addr = strtoul(tok, NULL, 16);
	    tok = strtok(NULL, DELIM);
	    if (tok != NULL && strlen(tok) != 0) len = strtoul(tok, NULL, 16);
	}
	GnuPlot(addr, len, map);
	break;
    case 'K' :	// program Si5338 with a register file
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
	addr = strtoul(tok, NULL, 16) & 3;
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
	ConfSI5338(map, addr, tok);
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
	    if (!quiet) printf("L2C[%5.5X] <= %2.2X\n", addr, N);
	}
	break;
    case 'M':	// Map address length
        tok = strtok(NULL, DELIM);
        if (tok == NULL || strlen(tok) == 0) {
           printf("VME region [%8.8X-%8.8X] is mapped at local address %p\n",
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
	Map(addr, len, map, fd, master);
	break;
    case 'N':
        if (map->ptr == NULL) {
    	    printf("Map some region first.\n");
	    break;
	}
	if (map->len < 0xC0000) {
	    printf("Mapped length (%8.8X) too small. We need 0xC0000 at least\n",map->len);
	    break;
	}
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
	addr = strtoul(tok, NULL, 16) & 3;
	tok = strtok(NULL, DELIM);
	if (tok == NULL) {
	    Help();
	    break;
	}
	N = strtoul(tok, NULL, 16);
	if (toupper(tok[strlen(tok) - 1]) == 'S') N = -N;
	tok = strtok(NULL, DELIM);
	GetEvents(map, addr, N, tok);
	break;	
    case 'P':	// Print [address [length]]
        if (map->ptr == NULL) {
	    printf("Map some region first.\n");
	    break;
	}
	addr = 0;
	len = 0x200;
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
	    printf("DAC SPI registers (%8.8X) above the mapped length (%8.8X)\n", DACSPI+8, map->len);
	    break;
	}
	N = strtoul(tok, NULL, 16) & 0xFFFF;
	M = -1;
	tok = strtok(NULL, DELIM);
	if (tok != NULL) {
	    M = strtoul(tok, NULL, 16) & 0xFFFF;
	}
	DACWrite(map, N);
	if (M >= 0) DACWrite(map, M);
	if (!quiet) {
	    printf("DAC <= %4.4X", N);
	    if (M >= 0) printf(" %4.4X", M);
	    printf("\n");
	}
	break;
    case 'T' :	// test memory
        if (map->ptr == NULL) {
    	    printf("Map the region first. Most likely you need:\n\tM ADC16000 2000\n");
	    break;
	}
	addr = 0;
	len = MEMSIZE;	// 128M dwords
	blen = 32;
	wp = 0;
	rp = 0;
	tok = strtok(NULL, DELIM);
	if (tok != NULL && strlen(tok) != 0) {
	    addr = strtoul(tok, NULL, 16);
	    tok = strtok(NULL, DELIM);
	    if (tok != NULL && strlen(tok) != 0) {
		len = strtoul(tok, NULL, 16);
		tok = strtok(NULL, DELIM);
		if (tok != NULL && strlen(tok) != 0) {
		    blen = strtoul(tok, NULL, 16);
		    tok = strtok(NULL, DELIM);
		    if (tok != NULL && strlen(tok) != 0) {
			wp = strtoul(tok, NULL, 16);
			if (wp < 0) wp = 0;
			if (wp > 5) wp = 5;
			tok = strtok(NULL, DELIM);
			if (tok != NULL && strlen(tok) != 0) {
			    rp = strtoul(tok, NULL, 16);
			    if (rp < 0) rp = 0;
			    if (rp > 1) rp = 1;
			}
		    }
		}
	    }
	}
	if (addr+len > MEMSIZE) len = MEMSIZE - addr;
	if (blen > len) blen = len;
	if (blen < 1 ) blen = 1;
	if (blen > 32) blen = 32;
	if (master->aspace == VME_A32) {
	    MemTestRegs(map, addr, len, blen, wp, rp);
	} else if (master->aspace == VME_A64) {
	    MemTestWB(map, addr, len);
	} else {
	    printf("ERROR: To run memory tests address space must be A64 (WB) or A32 (wb_tmem regs)\n");
	}
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
	    if (!quiet) printf("ICX[%4.4X] <= %4.4X\n", addr, N);
	}
	break;
    default:
	printf("Unknown command \"%c\"\n", toupper(tok[0]));
    }
    return 0;
}

int SpaceFromString(const char *str, u32 *aspace)
{
    int rc = 0;

    if (strcasecmp(str, "A16") == 0) *aspace = VME_A16;
    else if (strcasecmp(str, "A24") == 0) *aspace = VME_A24;
    else if (strcasecmp(str, "A32") == 0) *aspace = VME_A32;
    else if (strcasecmp(str, "A64") == 0) *aspace = VME_A64;
    else if (strcasecmp(str, "CRCSR") == 0) *aspace = VME_CRCSR;
    else rc = 1;

    return rc;
}

const char* StringFromSpace(u32 aspace)
{
    switch(aspace)
    {
    case VME_A16: return "A16";
    case VME_A24: return "A24";
    case VME_A32: return "A32";
    case VME_A64: return "A64";
    case VME_CRCSR: return "CRCSR";
    default: return "UNKNOWN";
    }
}

int WidthFromString(const char *str, u32 *dwidth)
{
    int rc = 0;

    if (strcasecmp(str, "D8") == 0) *dwidth = VME_D8;
    else if (strcasecmp(str, "D16") == 0) *dwidth = VME_D16;
    else if (strcasecmp(str, "D32") == 0) *dwidth = VME_D32;
    else if (strcasecmp(str, "D64") == 0) *dwidth = VME_D64;
    else rc = 1;

    return rc;
}

const char* StringFromWidth(u32 dwidth)
{
    switch(dwidth)
    {
    case VME_D8: return "D8";
    case VME_D16: return "D16";
    case VME_D32: return "D32";
    case VME_D64: return "D64";
    default: return "UNKNOWN";
    }
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
    struct vme_master master;
    int i, j;
 
    master.enable = 1;
    master.aspace = VME_A32;
    master.cycle = VME_USER | VME_DATA;
    master.dwidth = VME_D32;

    for (i=1; i<argc; i++) {
	if (argv[i][0] == '-') switch (toupper(argv[i][1])) {
	case 'H':
	    Help();
	    goto Quit;
	case 'W':
            if (WidthFromString(&argv[i][2], &master.dwidth) != 0)
            {
		printf("Unknown mode: %s\n", argv[i]);
		Help();
		goto Quit;
	    }
	    break;
	case 'Q':
	    quiet = 1;
	    break;
	case 'S':
            if (SpaceFromString(&argv[i][2], &master.aspace) != 0)
            {
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
	printf("\n\n\t\tManual VME controller: %s - %s\n\t\t\tSvirLex 2012\n\n",
            StringFromSpace(master.aspace), StringFromWidth(master.dwidth));
//	Open VME
    fd = open("/dev/bus/vme/m0", O_RDWR);
    if (fd < 0) {
	printf("FATAL - can not open VME - %m\n");
        printf("Try running:\n\tmodprobe vme\n\tmodprobe vme_tsi148\n\tmodprobe vme_user bus=0\n");
	return fd;
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
		Process(tok, fd, &map, &master);
	    } else {
		printf("The single operation is too long: %s\n", ptr);
		break;
	    }
	}
    } else {
	for(;;) {
	    if (cmd) free(cmd);
	    cmd = readline("VmeBur (H-help)>");
	    if (cmd == NULL)
            {
                printf("\n");
                break;
            }
	    if (strlen(cmd) == 0) continue;
	    add_history(cmd);
	    if (Process(cmd, fd, &map, &master)) break;
//          TODO: provide error detection interface for vme_user
//	    rc = VME4L_BusErrorGet(fd, &spcr, &vmeaddr, 1 );
//	    if (rc) printf("VME BUS ERROR: rc=%d @ spc=%d addr=0x%X\n", rc, spcr, vmeaddr);
	}
    }
Quit:
    
//	Close VME
    if (map.mmap_ptr != NULL) munmap(map.mmap_ptr, map.len);
    close(fd);
    if (cmd) free(cmd);
    return 0;
}
