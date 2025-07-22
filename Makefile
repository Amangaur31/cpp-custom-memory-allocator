# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

# Target executable name
TARGET = allocator

# Source files
SOURCES = main.cpp

# Default target
all: $(TARGET)

# Link the program
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# Clean up build files
clean:
	rm -f $(TARGET)

.PHONY: all clean
