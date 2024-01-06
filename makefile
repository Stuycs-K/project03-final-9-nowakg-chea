compile: server_compile client_compile
server: server_compile client_compile
	@./server
client: client_compile server_compile
	@./client
server_compile: server.o role.o
	@gcc -o server server.o role.o
client_compile: client.o role.o input.o
	@gcc -o client client.o role.o input.o
server.o: server.c role.h
	@gcc -c -Wall server.c
client.o: client.c
	@gcc -c -Wall client.c
input.o: input.c
	@gcc -c -Wall input.c
role.o: role.c
	@gcc -c -Wall role.c
clean:
	@rm -f *.o
	@rm -f ./server
	@rm -f ./client
