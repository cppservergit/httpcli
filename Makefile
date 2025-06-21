CXX = g++
CXXFLAGS = -O2 -std=c++23 -Wall -Wextra -pthread -I./include
LDFLAGS = -lcurl

TARGET = test
SRCS = src/main.cpp src/http_client.cpp
OBJS = $(SRCS:.cpp=.o)

ifeq ($(OS),Windows_NT)
	RM = del /F /Q
	TARGET_EXE = $(TARGET).exe
	TARGET_EXE_WIN = $(subst /,\\,$(TARGET_EXE))
	OBJS_WIN = $(subst /,\\,$(OBJS))
	TEST_BIN = $(TARGET_EXE_WIN)
else
	RM = rm -f
	TARGET_EXE = $(TARGET)
	TEST_BIN = ./$(TARGET_EXE)
endif

all: $(TARGET_EXE)

$(TARGET_EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET_EXE) $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET_EXE)
	@echo Running unit tests...
	$(TEST_BIN)

clean:
ifeq ($(OS),Windows_NT)
	$(RM) $(TARGET_EXE_WIN) $(OBJS_WIN)
else
	$(RM) $(TARGET_EXE) $(OBJS)
endif
