# Makefile – Automated Build Script

# Variables
CMAKE_BUILD_DIR := build
CMAKE_FLAGS     :=
MAKE_FLAGS      := -j

MAKEFLAGS += --no-print-directory

# Default target
.PHONY: all
all: build

# Create build directory, run CMake and Make
.PHONY: build
build:
	@echo ">> Entering in the build funciion..."
	@echo ">> Cleaning previous build directory..."
	@rm -rf $(CMAKE_BUILD_DIR)
	@echo ">> Creating build directory..."
	@mkdir -p $(CMAKE_BUILD_DIR)
	@echo ">> Running CMake..."
	@cd $(CMAKE_BUILD_DIR) && cmake .. $(CMAKE_FLAGS)
	@echo ">> Compiling..."
	@cd $(CMAKE_BUILD_DIR) && make $(MAKE_FLAGS)

# Run the compiled executable (example invocation; override as needed)
# Usage: make run N=<rows> M=<cols> I=<iterations> S=<seed> P=<nprocs>
.PHONY: run
run:
ifndef N
	$(error Please specify N=<rows>)
endif
ifndef M
	$(error Please specify M=<cols>)
endif
ifndef I
	$(error Please specify I=<iterations>)
endif
ifndef P
	$(error Please specify P=<nprocs>)
endif
	@echo ">> Running game_of_life with $(P) processes"
	@cd $(CMAKE_BUILD_DIR) && mpirun -np $(P) ./game_of_life -n $(N) -m $(M) -i $(I) $(if $(S),-s $(S),)

# Remove all build files
.PHONY: clean
clean:
	@echo ">> Removing all build files..."
	@rm -rf $(CMAKE_BUILD_DIR)

# Show help
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make           → Alias for 'make build'."
	@echo "  make build     → Clean, configure, and compile the project."
	@echo "  make run       → Run the executable. Requires N, M, I, P (and optional S)."
	@echo "  make clean     → Remove all build files."
	@echo "  make help      → Show this help message."
