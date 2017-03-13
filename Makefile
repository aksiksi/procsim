CC=g++
LFLAGS=-std=c++11 -Wall
CFLAGS=-c -std=c++11 -g -Wall
OBJ=obj

DEPS=$(OBJ)/util.o $(OBJ)/lru.o $(OBJ)/victim.o $(OBJ)/block.o $(OBJ)/cache.o
BIN=cachesim

.PHONY: clean

$(OBJ)/%.o: src/%.cpp
	$(CC) $(CFLAGS) $^ -o $@

%: src/%.cpp $(DEPS)
	$(CC) $(LFLAGS) $^ -o $@

default: $(CACHESIM) $(CACHEOPT)

clean:
	rm -f $(OBJ)/* $(CACHESIM)
