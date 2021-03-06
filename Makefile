CC=g++
LFLAGS=-std=c++11
CFLAGS=-c -O3 -std=c++11
OBJ=obj
INCLUDE=-Iinclude
HEADERS=include

DEPS=$(OBJ)/util.o $(OBJ)/pipeline.o $(OBJ)/predictor.o
PROCSIM=procsim
PROCOPT=procopt

$(OBJ)/%.o: src/%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) $^ -o $@

%: src/%.cpp $(DEPS)
	$(CC) $(INCLUDE) $(LFLAGS) $^ -o $@

default: $(PROCSIM)

.PHONY: clean archive

clean:
	rm -f $(OBJ)/* $(PROCSIM)

archive:
	tar -cvf project2_aksiksi3.tar.gz project2-report.pdf README.txt src/ obj/ include/ Makefile traces/*.trace.out
