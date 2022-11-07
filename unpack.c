#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <stdio.h>
#include "kycg.h"

static int usage() {
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: kycg unpack [options] <in.cg>\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "    -v        verbose\n");
  fprintf(stderr, "    -a        all samples\n");
  fprintf(stderr, "    -c        chunk process\n");
  fprintf(stderr, "    -b        begin (first sample index, default to first sample)\n");
  fprintf(stderr, "    -e        end (last sample index, default to last sample)\n");
  fprintf(stderr, "    -s        chunk size (default 1M)\n");
  fprintf(stderr, "    -h        This help\n");
  fprintf(stderr, "\n");

  return 1;
}

static void print_cg1(cgdata_t *cg, uint64_t i) {
  switch (cg->fmt) {
  case '0': {
      fputc(((cg->s[i>>3]>>(i&0x7))&0x1)+'0', stdout);
      break;
  }
  case '1': {
    fputc(cg->s[i], stdout);
    break;
  }
  case '3': {
    uint64_t *s = (uint64_t*) cg->s;
    fprintf(stdout, "%"PRIu64"\t%"PRIu64"",s[i]>>32,s[i]<<32>>32);
    break;
  }
  case '4': {
    if (((float_t*) (cg->s))[i]<0) {
      fputs("NA", stdout);
    } else {
      fprintf(stdout, "%1.3f", ((float_t*) (cg->s))[i]);
    }
    break;
  }
  case '5': {
    if (cg->s[i] == 2) {
      fputs("NA", stdout);
    } else {
      fputc(cg->s[i]+'0', stdout);
    }
    break;
  }
  case '6': {
    uint64_t *s = (uint64_t*) cg->s;
    fprintf(stdout, "%"PRIu64, s[i]);
    break;
  }
  default: usage(); wzfatal("Unrecognized format: %c.\n", cg->fmt);
  }
}

static void print_cg(cgdata_t *cg) {

  cgdata_t expanded = {0};
  decompress(cg, &expanded);
  uint64_t i;
  for (i=0; i<expanded.n; ++i) {
    print_cg1(&expanded, i); fputc('\n', stdout);
  }
  free(expanded.s);
}

static void print_cgs_chunk(cgdata_v *cgs, uint64_t s) {
  uint64_t i,m, k, kn = cgs->size;
  cgdata_t expanded = {0};
  decompress(ref_cgdata_v(cgs, 0), &expanded);
  uint64_t n = expanded.n;
  cgdata_t *sliced = calloc(kn, sizeof(cgdata_t));
  for (m=0; m <= n/s; ++m) {
    for (k=0; k<kn; ++k) {
      decompress(ref_cgdata_v(cgs, k), &expanded);
      slice(&expanded, m*s, (m+1)*s-1, &sliced[k]);
    }
    for (i=0; i<sliced[0].n; ++i) {
      for (k=0; k<kn; ++k) {
        if(k) fputc('\t', stdout);
        print_cg1(&sliced[k],i);
      }
      fputc('\n', stdout);
    }
  }

  for (k=0; k<kn; ++k) free(sliced[k].s);
  free(expanded.s); free(sliced);
}

static void print_cgs(cgdata_v *cgs) {
  uint64_t i, k, kn = cgs->size;
  cgdata_t *expanded = calloc(kn, sizeof(cgdata_t));
  for (k=0; k<kn; ++k)
    decompress(ref_cgdata_v(cgs, k), expanded+k);
  for (i=0; i<expanded[0].n; ++i) {
    for (k=0; k<kn; ++k) {
      if(k) fputc('\t', stdout);
      print_cg1(expanded+k, i);
    }
    fputc('\n', stdout);
  }
  for (k=0; k<kn; ++k) free(expanded[k].s);
  free(expanded);
}

int main_unpack(int argc, char *argv[]) {

  int c, verbose = 0, read_all = 0, chunk = 0;
  int64_t beg = -1, end = -1;
  uint64_t chunk_size = 1000000;
  while ((c = getopt(argc, argv, "cs:b:e:avh"))>=0) {
    switch (c) {
    case 'c': chunk = 1; break;
    case 's': chunk_size = atoi(optarg); break;
    case 'v': verbose = 1; break;
    case 'b': beg = atoi(optarg)-1; break;
    case 'e': end = atoi(optarg)-1; break;
    case 'a': read_all = 1; break;
    case 'h': return usage(); break;
    default: usage(); wzfatal("Unrecognized option: %c.\n", c);
    }
  }

  if (optind + 1 > argc) { 
    usage(); 
    wzfatal("Please supply input file.\n"); 
  }

  cgfile_t cgf = open_cgfile(argv[optind]);
  if (beg >= 0 || end >= 0 || read_all) {
    cgdata_v *cgs = read_cgs(&cgf, beg, end);
    if (chunk) print_cgs_chunk(cgs, chunk_size);
    else print_cgs(cgs);
    uint64_t i;
    for (i=0; i<cgs->size; ++i) free(ref_cgdata_v(cgs,i)->s);
    free_cgdata_v(cgs);
  } else {
    cgdata_t cg = read_cg(&cgf);
    print_cg(&cg);
    free(cg.s);
  }
  gzclose(cgf.fh);
  
  return 0;
}
