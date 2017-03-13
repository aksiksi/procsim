CC=g++
LFLAGS=-std=c++11 -Wall
CFLAGS=-c -std=c++11 -g -Wall
OBJ=obj
INCLUDE=-Iinclude

DEPS=$(OBJ)/util.o
PROCSIM=procsim

.PHONY: clean

$(OBJ)/%.o: src/%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) $^ -o $@

%: src/%.cpp $(DEPS)
	$(CC) $(INCLUDE) $(LFLAGS) $^ -o $@

default: $(PROCSIM)

clean:
	rm -f $(OBJ)/* $(PROCSIM)
