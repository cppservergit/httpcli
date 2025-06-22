# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -pedantic -O2 -Isrc -Iinclude

# File layout
SRC_DIR = src
INC_DIR = include
SRC = $(SRC_DIR)/http_client.cpp
HDR = $(INC_DIR)/http_client.hpp
OBJ = $(SRC:.cpp=.o)
LIB = libhttpclient.a

# Test configuration
TEST_SRC = $(SRC_DIR)/main.cpp

ifeq ($(OS),Windows_NT)
    TEST_BIN = test.exe
    RM = del /Q /F
    OBJ_WIN = $(subst /,\\,$(OBJ))
else
    TEST_BIN = test
    RM = rm -f
endif

all: $(LIB)

$(OBJ): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(OBJ)
	ar rcs $@ $^

$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $< -L. -lhttpclient -lcurl

test: $(TEST_BIN)

clean:
ifeq ($(OS),Windows_NT)
	$(RM) $(OBJ_WIN) $(LIB) $(TEST_BIN)
else
	$(RM) $(OBJ) $(LIB) $(TEST_BIN)
endif
