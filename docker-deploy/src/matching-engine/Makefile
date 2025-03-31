CC=g++
CFLAGS=-O3
LIBS=-ltinyxml2 -lpqxx -lpq
DEPS=DatabaseTransactions.h MatchingEngineServer.h TcpConnection.h
OBJECTS=DatabaseTransactions.o main.o MatchingEngineServer.o TcpConnection.o

all: main

main: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< 

clean:
	rm -f main *.o 