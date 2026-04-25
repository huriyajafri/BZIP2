CC = gcc
CFLAGS = -Iinclude

SOURCES = src/main.c src/block.c src/rle.c src/bwt.c src/config.c
TARGET = program

.PHONY: all clean run help

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(SOURCES) $(CFLAGS) -o $(TARGET)

clean:
	del /Q program.exe 2>nul || rm -f program.exe
	del /Q src\*.o 2>nul || rm -f src/*.o

run: $(TARGET)
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  make       - Build the program"
	@echo "  make run   - Build and run the program"
	@echo "  make clean - Remove built files"
