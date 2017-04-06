CC=g++
LFLAGS=-std=c++11
CFLAGS=-c -g -std=c++11
OBJ=obj
INCLUDE=-Iinclude

DEPS=$(OBJ)/util.o $(OBJ)/pipeline.o $(OBJ)/predictor.o
PROCSIM=procsim

$(OBJ)/%.o: src/%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) $^ -o $@

%: src/%.cpp $(DEPS)
	$(CC) $(INCLUDE) $(LFLAGS) $^ -o $@

default: $(PROCSIM)

.PHONY: clean archive

clean:
	rm -f $(OBJ)/* $(PROCSIM)

archive:
	tar -cvf project2_aksiksi3.tar.gz README.txt src/ obj/ include/ Makefile
