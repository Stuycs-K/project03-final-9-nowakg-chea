#include "server.h"
#include "input.h"
#include "role.h"
#include "player.h"

/*Connect to the server
 *return the to_server socket descriptor
 *blocks until connection is made.*/
int client_tcp_handshake(char * server_address) {
  struct addrinfo *hints, *results;
  hints = malloc(sizeof(struct addrinfo));
  hints->ai_family = AF_INET;
  hints->ai_socktype = SOCK_STREAM;
  //getaddrinfo
  getaddrinfo(server_address, PORT, hints, &results);
  //printf("getaddrinfo\n");
  int serverd;
  //create the socket
  serverd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);
  //printf("socket\n");
  //connect to the server
  connect(serverd, results->ai_addr, results->ai_addrlen);
  //printf("connect\n");
  free(hints);
  freeaddrinfo(results);

  return serverd;
}

int main(int argc, char *argv[] ) {
  char buffer[BUFFER_SIZE];
  char* IP = "127.0.0.1";
  if(argc>1){
    IP=argv[1];
    //printf("'%s'\n", IP);
  }
  int server_socket = client_tcp_handshake(IP);

  query("Enter a name:", buffer, BUFFER_SIZE);
  write(server_socket, buffer, BUFFER_SIZE);
  printf("Please wait for the server to start the game...\n");


  //for now, just have the client read from the server
  while(1){
    //read will be 0 bytes when the server doesn't send a message
    if(read(server_socket, buffer, BUFFER_SIZE) > 0){
      printf("%s\n", buffer);
    }
  }

  return 0;
}
