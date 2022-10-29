/* View .tbi with .tbk
 * 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2022 Wanding.Zhou@pennmedicine.upenn.edu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 **/

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "kycg.h"

const int unit_base[40] = {
  0,  1,  1,  4,  4,  8,  8,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  8,
  8,  0,  0,  0,  0,  0,  0,  0
};

int main_pack(int argc, char *argv[]);
int main_overlap(int argc, char *argv[]);
int main_unpack(int argc, char *argv[]);
int main_split(int argc, char *argv[]);
int main_dim(int argc, char *argv[]);
int main_chunk(int argc, char *argv[]);
int main_chunkchar(int argc, char *argv[]);

static int usage()
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Program: kycg (KnowYourCG)\n");
  fprintf(stderr, "Version: %s\n", PACKAGE_VERSION);
  fprintf(stderr, "Contact: Wanding Zhou<wanding.zhou@pennmedicine.upenn.edu>\n\n");
  fprintf(stderr, "Usage:   kycg <command> [options]\n\n");
  fprintf(stderr, "Command:\n");
  fprintf(stderr, "     pack         pack data to cg file\n");
  fprintf(stderr, "     overlap      compute overlap of cg files\n");
  fprintf(stderr, "     unpack       unpack data\n");
  fprintf(stderr, "     dim          data dimension\n");
  fprintf(stderr, "     split        split mult-sample data to single-sample data\n");
  fprintf(stderr, "     chunk        chunk data to smaller fragments\n");
  fprintf(stderr, "     chunkchar    chunk txt to smaller fragments\n");
  fprintf(stderr, "\n");

  return 1;
}

int main(int argc, char *argv[]) {
  int ret;
  if (argc < 2) return usage();
  if (strcmp(argv[1], "pack") == 0) ret = main_pack(argc-1, argv+1);
  else if (strcmp(argv[1], "overlap") == 0) ret = main_overlap(argc-1, argv+1);
  else if (strcmp(argv[1], "unpack") == 0) ret = main_unpack(argc-1, argv+1);
  else if (strcmp(argv[1], "split") == 0) ret = main_split(argc-1, argv+1);
  else if (strcmp(argv[1], "dim") == 0) ret = main_dim(argc-1, argv+1);
  else if (strcmp(argv[1], "chunk") == 0) ret = main_chunk(argc-1, argv+1);
  else if (strcmp(argv[1], "chunkchar") == 0) ret = main_chunkchar(argc-1, argv+1);
  /* else if (strcmp(argv[1], "bundle") == 0) ret = main_bundle(argc-1, argv+1); */
  else {
    fprintf(stderr, "[main] unrecognized command '%s'\n", argv[1]);
    return 1;
  }

  fflush(stdout);             /* not enough for remote file systems */
  fclose(stdout);

  return ret;
}
