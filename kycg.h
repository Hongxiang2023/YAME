#ifndef _KYCG_H
#define _KYCG_H

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <limits.h>
#include <inttypes.h>
#include <wordexp.h>
#include "wzmisc.h"
#include "wzmisc.h"
#include "wzbed.h"
#include "bgzf.h"
#include "khash.h"

KHASH_MAP_INIT_STR(index, int64_t)  
#define index_t khash_t(index)
#define destroyIndex(idx) kh_destroy(index, idx)
#define DELIMITER "\t"

#define PACKAGE_VERSION "0.1.20221020"
#define CGSIG 266563789635

typedef struct cgdata_t {
  uint8_t *s;
  uint64_t n; /* number of bytes, except for fmt 0, which is sub-byte you need the actual length*/
  int compressed;
  char fmt;
} cgdata_t;

DEFINE_VECTOR(cgdata_v, cgdata_t)

/*************************************
 ** Loads an index from a file.      **
 *************************************
 * The index file should be in the following format:
 *     sample1\tindex1
 *     sample2\tindex2
 *     ...
 *
 * @param fname_cg The base filename for the index file.
 * @return A pointer to the loaded index hash table (index_t) if successful, or NULL on failure.
 *         The caller is responsible for freeing the memory.
 */
index_t* loadIndex(char* fname_cg);

/*************************************
 ** Retrieves the value for a given   **
 ** sample name from the index.       **
 *************************************
 *
 * @param index The index hash table (index_t).
 * @param sample_name The sample name to retrieve the index for.
 * @return The index value for the given sample name if found, or -1 if not found.
 */
int64_t getIndex(index_t* index, char* sample_name);

static inline uint64_t cgdata_nbytes(cgdata_t *cg) {
  uint64_t n = 0;
  switch(cg->fmt) {
  case '0': n = (cg->n>>3)+1; break;
  default: n = cg->n;
  }
  return n;
}

/* unit size of uncompressed data */
static inline uint64_t cgdata_unit_size(cgdata_t *cg) {
  switch(cg->fmt) {
  case '3': return 8; break;
  case '4': return 4; break;
  case '5': return 1; break;
  case '6': return 8; break;
  default: return 1;
  }
  return 1;
}

static inline void free_cgdata(cgdata_t *cg) {
  if(cg->s) free(cg->s);
  free(cg);
}

void fmta_tryBinary2byteRLE_ifsmaller(cgdata_t *cg);

cgdata_t *fmt0_read_uncompressed(char *fname, int verbose);
void fmt0_decompress(cgdata_t *cg, cgdata_t *expanded);

cgdata_t *fmt1_read_uncompressed(char *fname, int verbose);
void fmt1_compress(cgdata_t *cg);
void fmt1_decompress(cgdata_t *cg, cgdata_t *expanded);

cgdata_t *fmt3_read_uncompressed(char *fname, int verbose);
void fmt3_compress(cgdata_t *cg);
void fmt3_decompress(cgdata_t *cg, cgdata_t *expanded);

cgdata_t *fmt4_read_uncompressed(char *fname, int verbose);
void fmt4_compress(cgdata_t *cg);
void fmt4_decompress(cgdata_t *cg, cgdata_t *expanded);

cgdata_t *fmt5_read_uncompressed(char *fname, int verbose);
void fmt5_compress(cgdata_t *cg);
void fmt5_decompress(cgdata_t *cg, cgdata_t *expanded);

cgdata_t *fmt6_read_uncompressed(char *fname, int verbose);
void fmt6_compress(cgdata_t *cg);
void fmt6_decompress(cgdata_t *cg, cgdata_t *expanded);

void decompress(cgdata_t *cg, cgdata_t *expanded);
void recompress(cgdata_t *cg);

static inline void slice(cgdata_t *cg, uint64_t beg, uint64_t end, cgdata_t *cg_sliced) {

  if (cg->compressed) {
    fprintf(stderr, "[%s:%d] Cannot slice compressed data.\n", __func__, __LINE__);
    fflush(stderr);
    exit(1);
  }
  if (end > cg->n-1) end = cg->n-1;
  if (end < beg) wzfatal("Slicing negative span.");

  cg_sliced->s = realloc(cg_sliced->s, (end-beg+1)*cgdata_unit_size(cg));
  memcpy(cg_sliced->s, cg->s+beg*cgdata_unit_size(cg), (end-beg+1)*cgdata_unit_size(cg));
  cg_sliced->n = end - beg + 1;
  cg_sliced->compressed = 0;
  cg_sliced->fmt = cg->fmt;
}

static inline void cgdata_write(char *fname_out, cgdata_t *cg, const char *mode, int verbose) {

  FILE *out;
  if (fname_out)
    out = fopen(fname_out, mode);
  else
    out = stdout;

  if (!cg->compressed) recompress(cg);
  uint64_t sig = CGSIG; fwrite(&sig, sizeof(uint64_t), 1, out);
  fwrite(&cg->fmt, sizeof(uint8_t), 1, out);
  fwrite(&cg->n, sizeof(uint64_t), 1, out);
  fwrite(cg->s, 1, cgdata_nbytes(cg), out);
  fclose(out);

  if (verbose) {
    fprintf(stderr, "[%s:%d] Stored as Format %c\n", __func__, __LINE__, cg->fmt);
    fflush(stderr);
  }
}

typedef struct cgfile_t {
  BGZF *fh;
  int n;                        /* number of samples read */
} cgfile_t;

static inline cgfile_t open_cgfile(char *fname) { /* for read */
  cgfile_t cgf = {0};
  cgf.fh = bgzf_open(fname, "r");
  if (cgf.fh == NULL) {
    fprintf(stderr, "Error opening file %s\n", fname);
    exit(1);
  }
  cgf.n = 0;
  return cgf;
}

static inline int read_cg_(cgfile_t *cgf, cgdata_t *cg) {
  cg->n = 0;
  uint64_t sig;
  int64_t size;
  if (cgf->fh->block_length == 0) bgzf_read_block(cgf->fh); /* somehow this is needed for concat'ed bgzipped files */
  size=bgzf_read(cgf->fh, &sig, sizeof(uint64_t));
  if(size != sizeof(uint64_t)) return 0;
  if (sig != CGSIG) wzfatal("Unmatched signature. File corrupted.\n");
  bgzf_read(cgf->fh, &(cg->fmt), sizeof(char));
  bgzf_read(cgf->fh, &(cg->n), sizeof(uint64_t));
  cg->s = realloc(cg->s, cgdata_nbytes(cg));
  bgzf_read(cgf->fh, cg->s, cgdata_nbytes(cg));
  cg->compressed = 1;
  cgf->n++;
  return 1;
}

/* static inline int read_cg_(cgfile_t *cgf, cgdata_t *cg) { */
/*   cg->n = 0; */
/*   uint64_t sig; */
/*   if(!gzfread(&sig, sizeof(uint64_t), 1, cgf->fh)) return 0; */
/*   if (sig != CGSIG) wzfatal("Unmatched signature. File corrupted.\n"); */
/*   gzfread(&(cg->fmt), sizeof(char), 1, cgf->fh); */
/*   gzfread(&(cg->n), sizeof(uint64_t), 1, cgf->fh); */
/*   cg->s = realloc(cg->s, cgdata_nbytes(cg)); */
/*   gzfread(cg->s, 1, cgdata_nbytes(cg), cgf->fh); */
/*   cg->compressed = 1; */
/*   cgf->n++; */
/*   return 1; */
/* } */

static inline cgdata_t read_cg(cgfile_t *cgf) {
  cgdata_t cg = {0};
  if (!read_cg_(cgf, &cg)) return cg;
  return cg;
}

/* static inline int read_cg_dry(cgfile_t *cgf) { */
/*   uint64_t sig; */
/*   if(!gzfread(&sig, sizeof(uint64_t), 1, cgf->fh)) { return 0; } */
/*   if(sig != CGSIG) { wzfatal("Unmatched signature. File corrupted.\n"); } */
/*   char fmt; */
/*   gzfread(&fmt, sizeof(char), 1, cgf->fh); */
/*   uint64_t n; */
/*   gzfread(&n, sizeof(uint64_t), 1, cgf->fh); */
/*   //fprintf(stdout, "%d\n", n); */
/*   //fflush(stdout); */
/*   gzseek(cgf->fh, n, SEEK_CUR); */
/*   return n; */
/* } */

/* this function is memory intensive if there are many samples */
static inline cgdata_v* read_cg_all(cgfile_t *cgf) {

  cgdata_v *cgs = init_cgdata_v(10);
  while (1) {
    cgdata_t cg = read_cg(cgf);
    if (cg.n >0) push_cgdata_v(cgs, cg);
    else break;
  }
  return cgs;
}

static inline cgdata_v* read_cgs(cgfile_t *cgf, int64_t beg, int64_t end) {

  if (beg < 0) beg = 0;
  if (end >= 0 && end < beg) wzfatal("End is smaller than beg");

  cgdata_v *cgs = init_cgdata_v(10);
  cgdata_t cg = {0};
  int64_t i=0;
  for (i=0; end<0 || i<=end; ++i) {
    read_cg_(cgf, &cg);
    if (i<beg) continue;
    if (cg.n>0) {
      (*next_ref_cgdata_v(cgs)) = cg;
      cg.s = NULL;
    } else {
      break;
    }
  }
  return cgs;
}

static inline cgdata_v* read_cgs_with_indices(cgfile_t *cgf, const int64_t* indices, int n) {
    cgdata_v *cgs = init_cgdata_v(10);
    cgdata_t cg = {0};

    for (int i = 0; i < n; i++) {
        int64_t index = indices[i];
        assert(index >= 0);

        // Reposition the file pointer using bgzf_seek
        assert(bgzf_seek(cgf->fh, index, SEEK_SET) == 0);
        read_cg_(cgf, &cg);
        if (cg.n > 0) {
            (*next_ref_cgdata_v(cgs)) = cg;
            cg.s = NULL;
        } else {
            break;
        }
    }

    return cgs;
}

static inline uint32_t compressMU32(uint64_t M, uint64_t U) {
  /* compress the M and U to 32-bit  */
  if (M > 0xffff || U > 0xffff) {
    uint64_t tmp;
    int im = 0; tmp=M; while(tmp>>16) { tmp>>=1; ++im; }
    int iu = 0; tmp=U; while(tmp>>16) { tmp>>=1; ++iu; }
    im = (im>iu ? im : iu);
    M>>=im; U>>=im;
  }
  return (uint32_t) (M<<16|U);
}

static inline uint64_t sumMUpair(uint64_t MU1, uint64_t MU2) {
  uint64_t M = (MU1>>32) + (MU2>>32);
  uint64_t U = (MU1&0xffffffff) + (MU2&0xffffffff);
  if (M > 0xffffffff || U > 0xffffffff) {
    uint64_t tmp;
    int im = 0; tmp=M; while(tmp>>32) { tmp>>=1; ++im; }
    int iu = 0; tmp=U; while(tmp>>32) { tmp>>=1; ++iu; }
    im = (im>iu ? im : iu);
    M>>=im; U>>=im;
  }
  return (M<<32|U);
}

static inline uint64_t MUbinarize(uint64_t MU) {
  uint64_t M = MU>>32;
  uint64_t U = MU&0xffffffff;
  if (M==0 && U==0) {
    return 0;
  } else if (M>=U) {
    return (1ul<<32);
  } else {
    return 1ul;
  }
}

#endif /* _KYCG_H */
