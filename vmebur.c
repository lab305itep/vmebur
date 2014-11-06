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

#include "vme_user.h"

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
#define SI5338ADR	0x70	// Si5338 I2C address
#define SITMOUT		100	// Si5338 timeout
#define ADCSPI		0	// ADC SPI controller registers base

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
    Configure Si5338 using register file and algorithm from fig. 9 of manual
*/
void ConfSI5338(VMEMAP *map, int N, char *fname)
{
    const unsigned char RegMask[512] = {
//		page 1
//	0     1     2     3     4     5     6     7     8     9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00,	//   0-  9
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	//  10- 19
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xFF,	//  20- 29
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0x1F,	//  30- 39
	0xFF, 0x7F, 0x3F, 0x00, 0x00, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF,	//  40- 49
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  50- 59
	0xFF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  60- 69
	0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  70- 79
	0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	//  80- 89
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 0xFF, 0xFF, 0xFF, 	//  90- 99
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0xFF, 0xFF, 0xFF, 	// 100-109
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 	// 110-119
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
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 220-229
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 230-239
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 	// 240-249
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
    printf("Configuring Si5338@block=%d with %s\n", N, fname);
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
	printf("# VME region [%8.8X-%8.8X] successfully mapped at %8.8X\n",
	    addr, addr + len - 1, map->ptr);
    }
    return rc;
}

int Process(char *cmd, int fd, VMEMAP *map, u32 dwidth)
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
	    switch (dwidth) {
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
	    switch (dwidth) {
	    case VME_D32:
	        map->ptr[addr/4] = SWAP(len);
	        printf("VME[%8.8X + %8.8X] <= %8.8X\n", map->addr, addr, len);
	        break;
	    case VME_D16:
	        ((unsigned short *)map->ptr)[addr/2] = SWAP2(len) & 0xFFFF;
	        printf("VME[%8.8X + %8.8X] <= %4.4X\n", map->addr, addr, len);
	        break;
	    case VME_D8:
	        ((unsigned char *)map->ptr)[addr] = len & 0xFF;
	        printf("VME[%8.8X + %8.8X] <= %2.2X\n", map->addr, addr, len);
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
	    printf("ADC[%1.1X:%4.4X] <= %2.2X\n", N, addr, len);
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

int GetMENSpace(u32 aspace, u32 dwidth, VME4L_SPACE *spc)
{
    int rc = 0;

    if ((aspace == VME_A16) && (dwidth == VME_D16)) *spc = VME4L_SPC_A16_D16;
    else if ((aspace == VME_A16) && (dwidth == VME_D32)) *spc = VME4L_SPC_A16_D32;
    else if ((aspace == VME_A24) && (dwidth == VME_D16)) *spc = VME4L_SPC_A24_D16;
    else if ((aspace == VME_A24) && (dwidth == VME_D32)) *spc = VME4L_SPC_A24_D32;
    else if ((aspace == VME_A32) && (dwidth == VME_D32)) *spc = VME4L_SPC_A32_D32;
    else if (aspace == VME_CRCSR) *spc = VME4L_SPC_MST7;
    else if ((aspace == VME_A64) && (dwidth == VME_D32)) *spc = VME4L_SPC_A64_D32;
    else rc = 1;

    return rc;
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
    VME4L_SPACE spc, spcr;
    vmeaddr_t vmeaddr;
    u32 aspace, dwidth;
    int i, j;
    int quiet = 0;

    aspace = VME_A32;
    dwidth = VME_D32;
    for (i=1; i<argc; i++) {
	if (argv[i][0] == '-') switch (toupper(argv[i][1])) {
	case 'H':
	    Help();
	    goto Quit;
	case 'W':
            if (WidthFromString(&argv[i][2], &dwidth) != 0)
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
            if (SpaceFromString(&argv[i][2], &aspace) != 0)
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

    if (GetMENSpace(aspace, dwidth, &spc) != 0)
    {
        printf("Unsupported aspace/dwidth combination: %s/%s\n",
            StringFromSpace(aspace), StringFromWidth(dwidth));
        return EXIT_FAILURE;
    }

    if (!quiet) 
	printf("\n\n\t\tManual VME controller: %s\n\t\t\tSvirLex 2012\n\n", VME4L_SpaceName(spc));
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
		Process(tok, fd, &map, dwidth);
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
	    if (Process(cmd, fd, &map, dwidth)) break;
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
