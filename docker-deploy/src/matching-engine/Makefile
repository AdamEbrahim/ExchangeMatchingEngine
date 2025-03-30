CC=g++
CFLAGS=-O3
LIBS=
DEPS=
OBJECTS=

all: main

main: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f main *.o 