# Module build rules

CC := i686-elf-gcc
CFLAGS := -c -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fPIC
INCLUDE := -Ikernel/include -Ilibc/include

TARGET := hello_module.o
SOURCE := hello_module.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(INCLUDE) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
