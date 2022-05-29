CC=gcc
CFLAGS=-I$(IDIR) -Wall -Wextra

IDIR =./include
ODIR=./build
SDIR = ./src

BIN=tracker.out

LIBS= -pthread

_DEPS = message.h queue.h sized_array.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o message.o queue.o sized_array.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm $(OBJ) $(BIN)




