#include "server.h"
#include "input.h"
#include "role.h"

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
  moveCursor(0, 30);
  char buffer[BUFFER_SIZE];
  char* IP = "127.0.0.1";
  if(argc>1){
    IP=argv[1];
    //printf("'%s'\n", IP);
  }
  int server_socket = client_tcp_handshake(IP);

  printf("Please wait for the server to start the game...\n");

  // query("Enter a name:", buffer, BUFFER_SIZE);
  // write(server_socket, buffer, BUFFER_SIZE);

  //client has to listen to stdin and the server
  fd_set read_fds;

  while(1){
    FD_ZERO(&read_fds);
    //add listen_socket and stdin to the set
    FD_SET(server_socket, &read_fds);
    //add the pipe file descriptor
    FD_SET(STDIN_FILENO, &read_fds);

    //get the STDIN and listen_socket to both be listened to
    int i = select(server_socket+1, &read_fds, NULL, NULL, NULL);

    //this is dumb but you need to check i before putting it in err or it will
    //run even if the ernno is 0
    if(i < 0) err(i, "server select/socket error CLIENT");

    if(FD_ISSET(server_socket, &read_fds)){
      int j = read(server_socket, buffer, BUFFER_SIZE);
      if(j == 0) return 0;
      printf("%s\n", buffer);
      colorText(COLOR_RESET);
      fflush(stdout);
    }

    if(FD_ISSET(STDIN_FILENO, &read_fds)){
      fgets(buffer, sizeof(buffer), stdin);
      buffer[strlen(buffer) - 1] = 0;
      //get rid of newline
      //printf("stdin: %s\n", buffer);
      write(server_socket, buffer, BUFFER_SIZE);
      moveCursor(0, 29);
      clearLine();
      colorText(COLOR_CYAN);
    }

  }

  return 0;
}
