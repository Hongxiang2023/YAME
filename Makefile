CC = gcc
CFLAGS = -W -Wall -finline-functions -fPIC -std=gnu99 -Wno-unused-result -O3
CLIB = -lncurses -lpthread -lz -lm
CF_OPTIMIZE = 1

OS := $(shell uname)
ifeq ($(OS),  Darwin)
	CFLAGS += -Wno-unused-function
else
	CLIB += -lrt -ltinfo
endif

INCLUDE = include

PROG = kycg

.PHONY: build
build: exportcf $(PROG)

exportcf:
	$(eval export CF_OPTIMIZE)

.PHONY: debug
debug: CF_OPTIMIZE := 0
debug: CFLAGS += -g # -pg
debug: CFLAGS := $(filter-out -O3,$(CFLAGS))
debug: build

#####################
##### libraries #####
#####################

LHTSLIB_DIR = htslib
LHTSLIB_INCLUDE = htslib/htslib
LHTSLIB = $(LHTSLIB_DIR)/libhts.a
$(LHTSLIB) :
	make -C $(LHTSLIB_DIR) libhts.a

###################
### subcommands ###
###################

%.o: %.c
	$(CC) -c $(CFLAGS) -I$(LUTILS_DIR) -I$(LHTSLIB_INCLUDE) $< -o $@

# overlap.o: overlap.c
# 	$(CC) -c $(CFLAGS) -I$(LUTILS_DIR) -I$(LHTSLIB_INCLUDE) $< -o $@

# chunk.o: chunk.c
# 	$(CC) -c $(CFLAGS) -I$(LUTILS_DIR) -I$(LHTSLIB_INCLUDE) $< -o $@

# header.o: header.c
# 	$(CC) -c $(CFLAGS) -I$(LUTILS_DIR) -I$(LHTSLIB_INCLUDE) $< -o $@

# bundle.o: bundle.c
# 	$(CC) -c $(CFLAGS) -I$(LUTILS_DIR) -I$(LHTSLIB_INCLUDE) $< -o $@


SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c, %.o, $(SOURCES))

LIBS=$(OBJECTS) $(LTHSLIB) # view.o chunk.o pack.o header.o bundle.o

kycg: $(LIBS)
	gcc $(CFLAGS) -o $@ *.o $(LTHSLIB) $(CLIB)


## clean just src
.PHONY: clean
clean :
	rm -f *.o kycg
	make -C $(LHTSLIB_DIR) clean
