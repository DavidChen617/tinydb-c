CC = gcc
CFLAGS = -Wall -Wextra -g

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = simpledb

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean