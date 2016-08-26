CC ?= gcc
CFLAGS = -std=gnu99 -Wall -g

OBJ := v4l2_api.o
EXEC := main

all: $(OBJ) $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(EXEC): main.c $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -rf $(EXEC) $(OBJ) pic.ppm
