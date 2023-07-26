#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cgdata.h"

static int is_nonnegative_int(char *s) {
  size_t i;
  for (i=0; i<strlen(s); ++i) {
    if (!isdigit(s[i])) return 0;
  }
  return 1;
}

static int fitMU(uint64_t *M, uint64_t *U, uint64_t nbits) {
  int modified = 0;
  uint64_t max = (1ul << nbits)-1;
  while ((*M) >= max || (*U) >= max) {
    (*M) >>= 1;
    (*U) >>= 1;
    modified = 1;
  }
  return modified;
}

static void pack_value(uint8_t *data, uint64_t value, uint8_t unit) {
  for (uint8_t i=0; i<unit; ++i) {
    data[i] = (value & 0xff);
    value >>= 8;
  }
}

static uint64_t unpack_value(uint8_t *data, uint8_t unit) {
  uint64_t v = 0;
  for (uint8_t i=0; i<unit; ++i) v |= data[i] << (8*i);
  return v;
}

static void f3_pack_mu(uint8_t *data, uint32_t M, uint32_t U, uint8_t unit) {
  if (unit == 1) {
    data[0] = ((M<<4) | (U&0xf));
  } else {
    if (unit > 8 || (unit&0x1)) {
      fprintf(stderr, "[%s:%d] Unit size (%u) can only be 1,2,4,6,8.\n", __func__, __LINE__, unit);
      fflush(stderr);
      exit(1);
    }
    pack_value(data, U, unit/2);
    pack_value(data+unit/2, M, unit/2);
  }
}

// note this function generate uin32_t not uint64_t. Please fix.
uint64_t f3_unpack_mu(cgdata_t *cg, uint64_t i) {
  uint64_t M = 0, U = 0;
  if (cg->unit == 1) {
    M = cg->s[i]>>4;
    U = cg->s[i]&0xf;
  } else {
    uint8_t *data = cg->s + cg->unit*i;
    uint8_t k = 0;
    for (uint8_t j=0; j<cg->unit/2; ++j) U |= (data[k++] << (8*j));
    for (uint8_t j=0; j<cg->unit/2; ++j) M |= (data[k++] << (8*j));
  }
  return (M<<32) | (U & ((1ul<<32)-1));
}

/* static inline uint32_t compressMU32(uint64_t M, uint64_t U) { */
/*   /\* compress the M and U to 32-bit  *\/ */
/*   if (M > 0xffff || U > 0xffff) { */
/*     uint64_t tmp; */
/*     int im = 0; tmp=M; while(tmp>>16) { tmp>>=1; ++im; } */
/*     int iu = 0; tmp=U; while(tmp>>16) { tmp>>=1; ++iu; } */
/*     im = (im>iu ? im : iu); */
/*     M>>=im; U>>=im; */
/*   } */
/*   return (uint32_t) (M<<16|U); */
/* } */

/* uncompressed: [ M (uint32_t) | U (uint32_t) ] */
cgdata_t* fmt3_read_uncompressed(char *fname, int verbose) {
  uint8_t unit = 8; // max size, zero loss
  gzFile fh = wzopen(fname, 1);
  char *line = NULL;
  uint8_t *s = NULL; uint64_t n = 0;
  char **fields; int nfields;
  while (gzFile_read_line(fh, &line) > 0) {
    line_get_fields(line, "\t", &fields, &nfields);
    if (nfields < 2) wzfatal("Number of fields <2. Abort.");
    if (!is_nonnegative_int(fields[0]) || !is_nonnegative_int(fields[1]))
      wzfatal("Field 1 or 2 is not a nonnegative integer.");
    uint64_t M = atol(fields[0]);
    uint64_t U = atol(fields[1]);
    s = realloc(s, (n+1)*unit);
    fitMU(&M, &U, 32);
    f3_pack_mu(s+n*unit, M, U, unit);
    n++;
    free_fields(fields, nfields);
  }
  free(line);
  wzclose(fh);
  if (verbose) {
    fprintf(stderr, "[%s:%d] Vector of length %lu loaded\n", __func__, __LINE__, n);
    fflush(stderr);
  }
  cgdata_t *cg = calloc(sizeof(cgdata_t),1);
  cg->s = s;
  cg->n = n;
  cg->compressed = 0;
  cg->fmt = '3';
  cg->unit = unit;
  return cg;
}

/* compressed: assume little endian, TODO: use endian swap
   2byte | U=M=0 -------------- = run len (14 bit) + 0 (2bit)
   1byte | U,M in [0,7] ------- = M (3bit) | U (3bit) + 1 (2bit)
   2byte | U,M in [0,127]------ = M (7bit) | U (7bit) + 2 (2bit)
   8byte | M,U in [128,2**31]-- = M (31bit) | U (31bit) + 3 (2bit)
*/
void fmt3_compress(cgdata_t *cg) {
  uint8_t *s = NULL;
  uint64_t n = 0;
  uint64_t i = 0;
  uint32_t l = 0;
  for (i=0; i<cg->n; i++) {
    uint64_t MU = f3_unpack_mu(cg, i);
    uint64_t M = MU>>32;
    uint64_t U = MU<<32>>32;
    if (M>0 || U>0 || l+2 >= 1<<14) {
      if (l>0) {
        s = realloc(s, n+2);
        *((uint16_t*) (s+n)) = (uint16_t) l<<2;
        n += 2;
        if (M>0 || U>0) l = 0;
        else l = 1;
      }
      if (M>0 || U>0) {
        if (M<7 && U<7) {
          s = realloc(s, n+1);
          s[n] = (M<<5) | (U<<2) | 0x1;
          n++;
        } else if (M<127 && U<127) {
          s = realloc(s, n+2);
          *((uint16_t*) (s+n)) = (M<<9) | (U<<2) | 0x2;
          n += 2;
        } else {
          fitMU(&M, &U, 31);
          s = realloc(s, n+8);
          *((uint64_t*) (s+n)) = (M<<33) | (U<<2) | 3ul;
          n += 8;
        }
      }
    } else {
      ++l;
    }
  }
  if (l>0) {
    s = realloc(s, n+2);
    *((uint16_t*) (s+n)) = (uint16_t) l<<2;
    n += 2;
  }
  free(cg->s);
  cg->s = s;
  cg->n = n;
  cg->compressed = 1;
}

static uint64_t get_data_length(cgdata_t *cg, uint8_t *unit) {
  uint8_t nbits = 1; // half unit nbits, M or U.
  uint64_t n = 0;
  for (uint64_t i=0; i < cg->n; ) {
    if ((cg->s[i] & 0x3) == 0) {
      n += (((uint16_t*) (cg->s+i))[0])>>2;
      i += 2;
    } else if ((cg->s[i] & 0x3) == 1) {
      uint64_t M = (cg->s[i])>>5;
      uint64_t U = ((cg->s[i])>>2) & 0x7;
      while (M >= (1ul << nbits) || U >= (1ul << nbits)) nbits++;
      n++; i++;
    } else if ((cg->s[i] & 0x3) == 2) {
      uint64_t M = (((uint16_t*) (cg->s+i))[0])>>2;
      uint64_t U = M & ((1<<7)-1);
      M >>= 7;
      while (M >= (1ul << nbits) || U >= (1ul << nbits)) nbits++;
      n++; i += 2;
    } else {
      uint64_t M = (((uint64_t*) (cg->s+i))[0])>>2;
      uint64_t U = M & ((1ul<<31)-1);
      M >>= 31;
      while (M >= (1ul << nbits) || U >= (1ul << nbits)) nbits++;
      n++; i += 8;
    }
  }
  if (nbits <= 0xf) *unit = 1;
  else *unit = (nbits>>2) + 1; // nbits*2/8
  return n;
}

void fmt3_decompress(cgdata_t *cg, cgdata_t *expanded) {
  uint8_t unit = 1;
  uint64_t n0 = get_data_length(cg, &unit);
  if (!cg->unit) cg->unit = unit; // use inferred max unit if unset
  uint8_t *s = calloc(cg->unit*n0, sizeof(uint8_t));
  uint64_t n = 0; uint64_t modified = 0;
  for (uint64_t i=0; i < cg->n; ) {
    if ((cg->s[i] & 0x3) == 0) {
      uint64_t l = unpack_value(cg->s+i, 2)>>2; // the length is 14 bits, so unit = 2
      memset(s+n*cg->unit, 0, cg->unit*l); n += l;
      i += 2;
    } else if ((cg->s[i] & 0x3) == 1) {
      uint64_t M = (cg->s[i])>>5;
      uint64_t U = ((cg->s[i])>>2) & 0x7;
      if (cg->unit == 1) modified += fitMU(&M, &U, 4);
      else modified += fitMU(&M, &U, (cg->unit>>1)*8);
      f3_pack_mu(s+(n++)*cg->unit, M, U, cg->unit);
      i++;
    } else if ((cg->s[i] & 0x3) == 2) {
      uint64_t M = unpack_value(cg->s+i, 2)>>2;
      uint64_t U = M & ((1ul<<7)-1);
      M >>= 7;
      if (cg->unit == 1) modified += fitMU(&M, &U, 4);
      else modified += fitMU(&M, &U, (cg->unit>>1)*8);
      f3_pack_mu(s+(n++)*cg->unit, M, U, cg->unit);
      i += 2;
    } else {
      uint64_t M = unpack_value(cg->s+i, 8)>>2;
      uint64_t U = M & ((1ul<<31)-1);
      M >>= 31;
      if (cg->unit == 1) modified += fitMU(&M, &U, 4);
      else modified += fitMU(&M, &U, (cg->unit>>1)*8);
      f3_pack_mu(s+(n++)*cg->unit, M, U, cg->unit);
      i += 8;
    }
  }
  assert(n0 == n);
  expanded->s = s;
  expanded->n = n;
  expanded->compressed = 0;
  expanded->fmt = '3';
  expanded->unit = cg->unit;
}
