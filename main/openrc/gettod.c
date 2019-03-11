#include <stdio.h>

void main(){
	unsigned long val;
	asm volatile("stck 0(%1)" : "=m" (val) : "a" (&val) : "cc");
	val = ((val >> 9) * 125) + (((val & 0x1ff) * 125) >> 9);
	printf("%lu", val / 1000000000UL - 2208988800);
}
