CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags readline 2>/dev/null)
LDFLAGS = $(shell pkg-config --libs readline 2>/dev/null || echo -lreadline)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = simpledb

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean