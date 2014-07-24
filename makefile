CC = gcc

CFLAGS  = -std=c99 \
-Wall -Wextra \
-march=native -Ofast -flto
LDFLAGS =

HDR = fbrec.h framebuffer.h gifsave89.h ttyrec.h  #fb2gif.h quant.h
SRC = fbrec.c #quant.c
DST = fbrec

all: $(DST)

$(DST): $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $@

clean:
	rm -f $(DST)
