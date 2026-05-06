CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -std=c11
LDFLAGS = -no-pie

TARGET  = utm
CSRC    = main.c
ASMSRC  = tm_engine.asm
COBJ    = $(CSRC:.c=.o)
ASMOBJ  = $(ASMSRC:.asm=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(COBJ) $(ASMOBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c tm_types.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.asm
	$(CC) -x assembler -c -g -o $@ $<

clean:
	rm -f $(TARGET) $(COBJ) $(ASMOBJ)

test: $(TARGET)
	@echo "=== T1 (lecture TM, empty tape) ==="
	./$(TARGET) --binary "010010001010011000101010010110001001001010011000100010001010" --run
	@echo ""
	@echo "=== T1 with input '11' (ACCEPT) ==="
	./$(TARGET) --binary "010010001010011000101010010110001001001010011000100010001010" --input "11" --run
	@echo ""
	@echo "=== T2 Goedel, input '1001' (ACCEPT - contains 00) ==="
	./$(TARGET) --binary "1010010100100110101000101001100010010100100110001010010100" --input "1001" --run
	@echo ""
	@echo "=== Decimal Goedel 1480103890654955658 ==="
	./$(TARGET) --decimal "1480103890654955658" --input "11" --run