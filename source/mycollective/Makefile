# Flags and Compiler
CC = mpicc
CFLAGS = -Wall -g -O3

# Directories
UTILS_DIR = utils

# Sources and 
SRCS = main.c mycollective.c $(UTILS_DIR)/utils.c
OBJS = $(SRCS:.c=.o)
TARGET = mycollective

# Default rule
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Clean rule
clean:
	rm -f $(TARGET) *.o $(UTILS_DIR)/*.o

# Phony targets
.PHONY: all clean
