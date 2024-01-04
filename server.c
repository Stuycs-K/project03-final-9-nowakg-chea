#include "server.h"
#include "role.h"

void err(int i, char*message){
  if(i < 0){
	  printf("Error: %s - %s\n",message, strerror(errno));
  	exit(1);
  }
}

static void sighandler(int signo) {
  if(signo == SIGCHLD) {
    int i;
    wait(&i);
  }
}

void subserver_logic(int client_socket){
  char buff[BUFFER_SIZE];
  read(client_socket, buff, BUFFER_SIZE);
  write(client_socket, buff, BUFFER_SIZE);
}

/*Accept a connection from a client
 *return the to_client socket descriptor
 *blocks until connection is made.
 */
int server_tcp_handshake(int listen_socket){
    int client_socket;
    socklen_t sock_size;
    struct sockaddr_storage client_address;
    sock_size = sizeof(client_address);

    //accept the client connection
    client_socket = accept(listen_socket,(struct sockaddr *)&client_address, &sock_size);

    return client_socket;
}

/*Create and bind a socket.
* Place the socket in a listening state.
*/
int server_setup() {
  //setup structs for getaddrinfo
  struct addrinfo *hints, *results;
  hints = malloc(sizeof(struct addrinfo));
  hints->ai_family = AF_INET;
  hints->ai_socktype = SOCK_STREAM;
  hints->ai_flags = AI_PASSIVE;
  getaddrinfo(NULL, PORT, hints, &results);

  //create the socket
  int clientd = socket(results->ai_family, results->ai_socktype, results->ai_protocol);

  //this code should get around the address in use error
  int yes = 1;
  int sockOpt =  setsockopt(clientd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  err(sockOpt,"sockopt  error");

  //bind the socket to address and port
  bind(clientd, results->ai_addr, results->ai_addrlen);

  //set socket to listen state
  listen(clientd, 10);

  //free the structs used by getaddrinfo
  free(hints);
  freeaddrinfo(results);

  return clientd;
}

void serverStart() {
  signal(SIGCHLD, sighandler);
  int listen_socket = server_setup();
  while(1) {
    int client_socket = server_tcp_handshake(listen_socket);
    pid_t p = fork();
    if(p == 0) {
      //printf("connected\n");
      subserver_logic(client_socket);
      return;
    }
  }
  return;
}

void main() {
  signal(SIGCHLD, sighandler);
  int listen_socket = server_setup();
  fd_set read_fds;
  char buffer[100];

  FD_ZERO(&read_fds);

  //add listen_socket and stdin to the set
  FD_SET(listen_socket, &read_fds);
  //add stdin's file desciptor
  FD_SET(STDIN_FILENO, &read_fds);

  int i = select(listen_socket+1, &read_fds, NULL, NULL, NULL);

  //if standard in, use fgets
  if (FD_ISSET(STDIN_FILENO, &read_fds)) {
    fgets(buffer, sizeof(buffer), stdin);
  }
  //if socket, accept the connection
  //assume this function works correctly
  if (FD_ISSET(listen_socket, &read_fds)) {
    client_socket = server_connect(listen_socket);
  }

  return;
}
