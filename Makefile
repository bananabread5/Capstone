
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g

TARGET = reservation_system
SERVER = reservation_server
CLIENT = reservation_client

OBJECTS = main.o seatmap.o storage.o

.PHONY: all clean

all: $(TARGET) $(SERVER) $(CLIENT)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS)

$(SERVER): server.o seatmap.o storage.o
	$(CC) $(CFLAGS) -o $@ server.o seatmap.o storage.o

$(CLIENT): client.o
	$(CC) $(CFLAGS) -o $@ client.o

main.o: main.c seatmap.h storage.h
	$(CC) $(CFLAGS) -c main.c

seatmap.o: seatmap.c seatmap.h
	$(CC) $(CFLAGS) -c seatmap.c

storage.o: storage.c storage.h seatmap.h
	$(CC) $(CFLAGS) -c storage.c

server.o: server.c seatmap.h storage.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

clean:
	rm -f *.o $(TARGET) $(SERVER) $(CLIENT)
