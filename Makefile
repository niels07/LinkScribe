CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 libcurl`
LIBS = `pkg-config --libs gtk+-3.0 libcurl` -lssl -lcrypto -luuid
OBJ = main.o gui.o ftp.o utils.o storage.o encryption.o
BIN = LinkScribe

all: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(CFLAGS) $(LIBS)

main.o: main.c
	$(CC) -c $(CFLAGS) main.c

gui.o: gui.c
	$(CC) -c $(CFLAGS) gui.c

ftp.o: ftp.c
	$(CC) -c $(CFLAGS) ftp.c

utils.o: utils.c
	$(CC) -c $(CFLAGS) utils.c

storage.o: storage.c
	$(CC) -c $(CFLAGS) storage.c

encryption.o: encryption.c
	$(CC) -c $(CFLAGS) encryption.c

clean:
	rm -f $(OBJ) $(BIN)
