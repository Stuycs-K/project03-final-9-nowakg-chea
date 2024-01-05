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

int main() {
  signal(SIGCHLD, sighandler);
  int listen_socket = server_setup();
  char buffer[BUFFER_SIZE];
  struct player players[15];
  int playerCount = 0;
  int joinPhase = 1;
  while(joinPhase) {
    int temp = server_tcp_handshake(listen_socket);
    read(temp, buffer, BUFFER_SIZE);
    if(buffer[0] == '\a') { //client sends this to start
      joinPhase = 0;
    } else {
      struct player* curr = players + playerCount;
      strcpy(curr->name, buffer);
      curr->sockd = temp;
      curr->alive = 1;
      ++playerCount;
    }
    if(playerCount == 15) joinPhase = 0;
  }
  for(int i = 0; i < playerCount; ++i) {
    printf("%s\n", players[i].name);
    write(players[i].sockd, "starting", 9);
  }
  return 0;
}
