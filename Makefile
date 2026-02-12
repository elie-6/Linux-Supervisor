# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread -Iinclude

# Source files (.c only!)
SRCS = src/config.c src/supervisor.c src/main.c src/logging.c src/cgroup.c 

# Object files in build/ folder
OBJS = $(patsubst src/%.c,build/src/%.o,$(SRCS))

# Executable
TARGET = Msupervisor

# Default build
all: $(TARGET)

# Link object files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile .c -> build/.o
build/src/%.o: src/%.c
	@mkdir -p $(dir $@)   # ensure directory exists
	$(CC) $(CFLAGS) -c $< -o $@

# Run program with config file
run: $(TARGET)
	./$(TARGET) supervisor.conf

# Clean build
clean:
	rm -f $(TARGET)
	rm -rf build
	mkdir -p build
