/* Block TEA decryption routine */

/* Based on btea routine at https://en.wikipedia.org/wiki/XXTEA */

/* Split encrypt and decrypt in separate routines */
/* Added base rounds argument */

#include <stdint.h>

#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

#if 0
void btea_encrypt(uint32_t *v, int n, int base_rounds,
		  uint32_t const key[4]) {
  uint32_t y, z, sum;
  unsigned p, rounds, e;
  /* Coding Part */
  rounds = base_rounds + 52/n;
  sum = 0;
  z = v[n-1];
  do {
    sum += DELTA;
    e = (sum >> 2) & 3;
    for (p=0; p<n-1; p++) {
      y = v[p+1]; 
      z = v[p] += MX;
    }
    y = v[0];
    z = v[n-1] += MX;
  } while (--rounds);
}
#endif

void btea_decrypt(uint32_t *v, int n, int base_rounds,
		  uint32_t const key[4]) {
  uint32_t y, z, sum;
  unsigned p, rounds, e;
  /* Decoding Part */
  rounds = base_rounds + 52/n;
  sum = rounds*DELTA;
  y = v[0];
  do {
    e = (sum >> 2) & 3;
    for (p=n-1; p>0; p--) {
      z = v[p-1];
      y = v[p] -= MX;
    }
    z = v[n-1];
    y = v[0] -= MX;
    sum -= DELTA;
  } while (--rounds);
}

