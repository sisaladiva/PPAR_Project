# Makefile for compiling mitm.c with MPI and OpenMP support

# Compiler
CC = mpicc

# Compiler flags
CFLAGS = -O3 -Wall -Wextra -ftree-vectorize -march=native -fopenmp -pg
LDFLAGS = -lm

# Target executable
TARGET = mitm

# Source files
SRCS = mitm.c

# # Object files
# OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean run

# Run target to execute the program with parameters
run: check_args
	@echo "Running the program with the following parameters:"
	@echo "NPROC (Number of processes): $(NPROC)"
	@echo "N: $(N)"
	@echo "C0: $(C0)"
	@echo "C1: $(C1)"
	@mpiexec -n $(NPROC) ./$(TARGET) --n $(N) --C0 $(C0) --C1 $(C1)

# Check if all required arguments are provided
check_args:
	@for arg in NPROC N C0 C1; do \
		if [ -z "$$(eval echo $$arg)" ]; then \
			echo "Error: 		Missing argument '$$arg' (value for --$$arg)."; \
			echo "Usage: 		make NPROC=<num_processes> N=<n_value> C0=<C0_value> C1=<C1_value> run"; \
			echo "For example: 	make NPROC=8 N=30 C0=b35de13b51802ec6 C1=1fae4ee8767747ef run"; \
			echo "Exiting..."; \
			exit 1; \
		fi \
	done
