#include "server.h"
#include "input.h"

void clientLogic(int server_socket){
  char buff[BUFFER_SIZE];
  printf("Enter a string:\n");
  fgets(buff, BUFFER_SIZE, stdin);
  write(server_socket, buff, BUFFER_SIZE);
  read(server_socket, buff, BUFFER_SIZE);
  printf("Processed string: %s", buff);
}

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
  char* IP = "127.0.0.1";
  if(argc>1){
    IP=argv[1];
    //printf("'%s'\n", IP);
  }
  int server_socket = client_tcp_handshake(IP);
  printf("client connected.\n");
  clientLogic(server_socket);
  return 0;
}
