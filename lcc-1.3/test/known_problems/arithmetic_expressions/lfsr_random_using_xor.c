#include <stdio.h>

int random_test_16bits() {
	unsigned short lfsr = 0xACE1u;
	unsigned bit;
	unsigned period = 0;
	do
	{
	  printf("%d: %d (%x)\n", period, lfsr, lfsr);
	  /* taps: 16 14 13 11; characteristic polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
	  bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
	  lfsr =  (lfsr >> 1) | (bit << 15);
	  ++period;
	} while(lfsr != 0xACE1u);

	return 0;
}



int random_test_4bits() {
	const unsigned short initial = 0x1u;
	unsigned short lfsr = initial;
	unsigned bit;
	unsigned period = 0;
	do
	{
	  printf("%d: %d (%x)\n", period, lfsr, lfsr);
	  /* taps: 4 3; characteristic polynomial: x^4 + x^3 + 1 */
	  bit  = ((lfsr >> 0) ^ (lfsr >> 1)) & 1;
	  lfsr =  (lfsr >> 1) | (bit << 3);
	  ++period;
	} while(lfsr != initial);

	return 0;
}

int main(){
	random_test_4bits();


}
