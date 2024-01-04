compile: server_compile client_compile
server: server_compile
	@./server
client: client_compile
	@./client
server_compile: server.o server.h role.h
	@gcc -o server server.o
client_compile: client.o client.h role.h
	@gcc -o client client.o
server.o: server.c server.h role.h
	@gcc -c server.c
client.o: client.c client.h role.h
	@gcc -c client.c
clean:
	@rm -f *.o
	@rm -f ./server
	@rm -f ./client
