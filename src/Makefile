# Makefile
CXX      := g++
CXXFLAGS := -std=c++17 -O0 -g
INCLUDE  := -I../include

SRC  := ProducerConsumerQueue.test.cpp
BIN  := test_pcq

.PHONY: all clean run

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $< -o $@

run: $(BIN)
	./$(BIN)

clean:
	$(RM) $(BIN)
