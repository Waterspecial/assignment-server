# Compiler g++
CXX := g++


CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -pedantic -O2

# Detect operating system for platform-specific flags
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)

    LDFLAGS := -pthread -lstdc++fs

    SECCOMP_CHECK := $(shell pkg-config --exists libseccomp 2>/dev/null && echo yes || echo no)
    ifeq ($(SECCOMP_CHECK),yes)
        LDFLAGS += -lseccomp
        CXXFLAGS += -DHAS_SECCOMP
    endif
else ifeq ($(UNAME_S),Darwin)
    LDFLAGS := -pthread
endif

# Source files and object files
SRC_DIR := src
BUILD_DIR := build
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Output binary name
TARGET := ass-server


# build the server
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link all object files into the final binary
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .cpp file into a .o object file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Build and run
run: all
	./$(TARGET)

.PHONY: all clean run
