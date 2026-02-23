# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -std=c11

# Target executable name
TARGET = myshell

# Default target
all: $(TARGET)

# Compile myshell
$(TARGET): myshell.c
	$(CC) $(CFLAGS) myshell.c -o $(TARGET)

# Clean compiled files
clean:
	rm -f $(TARGET)	