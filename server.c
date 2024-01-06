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


void sendMessage(char* message, struct player allPlayers[]){
   for (int n = 0; allPlayers[n].sockd != 0; n++){
    //strcpy(messageCopy, message);
    write(allPlayers[n].sockd, message, BUFFER_SIZE);
  }
}


int main() {
  signal(SIGCHLD, sighandler);
  int listen_socket = server_setup();
  char buffer[BUFFER_SIZE];
  char serverBuff[BUFFER_SIZE];
  struct player* allPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* townPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* mafiaPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* neutralPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  int playerCount = 0;
  int joinPhase = 1;

  //FD select system
  fd_set read_fds;

  //PLAYER JOINING
  while(joinPhase){
    FD_ZERO(&read_fds);
    //add listen_socket and stdin to the set
    FD_SET(listen_socket, &read_fds);
    //add stdin's file desciptor
    FD_SET(STDIN_FILENO, &read_fds);

    //get the STDIN and listen_socket to both be listened to
    int i = select(listen_socket+1, &read_fds, NULL, NULL, NULL);
    err(i, "server select/socket error");

    if(FD_ISSET(STDIN_FILENO, &read_fds)){
      //server should be able to start the game
      fgets(serverBuff, sizeof(serverBuff), stdin);
      if(strcmp(serverBuff, "/start\n") == 0 && playerCount >= 7){
        printf("Starting the game!\n");
        joinPhase = 0;
      }
    }

    if(FD_ISSET(listen_socket, &read_fds)){
      //temp is the joining player
      int temp = server_tcp_handshake(listen_socket);
      int bytes = read(temp, buffer, BUFFER_SIZE);
      if(bytes == 0) { //someone disconnected
        for(int i = 0; i < playerCount; ++i) {
          if(allPlayers[i].sockd == temp) { //if they disconnected
            bytes = -1;
          }
          if(bytes == -1) { //if we are after the gone player in the list
            if(i < playerCount - 1) allPlayers[i] = allPlayers[i+1];
          }
        }
        --playerCount;
      } else {
        struct player newPlayer;
        strcpy(newPlayer.name, buffer);
        newPlayer.sockd = temp;
        newPlayer.alive = 1;
        allPlayers[playerCount] = newPlayer;
        printf("playercount: %d\n", playerCount);
        ++playerCount;
        printf("Player connected: %s\n", newPlayer.name );
      }
    }

    if(playerCount == MAX_PLAYERS) joinPhase = 0;
  }

  printf("Players connected: \n");
  for(int i = 0; i < playerCount; ++i) {
    printf("%s\n", allPlayers[i].name);
  }
  sendMessage("you have been conneced to the server!", allPlayers);



  //ROLE DISTRIBUTION
  //figuring out how many of each (will go 610, 710, 720, 721, 821, 831, 832, 932, 942)
  int townCount = 6;
  int mafiaCount = 1;
  int neutralCount = 0;
  for(int i = 7; i < playerCount; ++i) {
    if(i % 3 == 1) ++townCount;
    if(i % 3 == 2) ++mafiaCount;
    if(i % 3 == 0) ++neutralCount;
  }
  
  for(int i = 0; i < playerCount; ++i) {
    printf("Player: %d\n", i);
    int team = -1, role = -1;
    int randFile = open("/dev/random", O_RDONLY);
    read(randFile, &team, sizeof(int));
    read(randFile, &role, sizeof(int));
    close(randFile);
    if(team < 0) team *= -1;
    if(role < 0) role *= -1;
    if(townCount > 0 && mafiaCount > 0 && neutralCount > 0) {
      team %= 3;
    } else if(townCount == 0 && mafiaCount > 0 && neutralCount > 0) {
      team %= 2; //0 or 1
      ++team; // 1 or 2
    } else if(townCount > 0 && mafiaCount == 0 && neutralCount > 0) {
      team %= 2; //0 or 1
      if(team == 1) ++team; //0 or 2
    } else if(townCount > 0 && mafiaCount > 0 && neutralCount == 0) {
      team %= 2; //0 or 1
    } else if(townCount > 0) {
      team = 0;
    } else if(mafiaCount > 0) {
      team = 1;
    } else if(neutralCount > 0) {
      team = 2;
    }
    printf("Team: %d\n", team);
    if(team == T_TOWN) {
      role %= R_MAXTOWNROLE + 1;
      --townCount;
      while(townPlayers[role].sockd != 0) {
        if(++role > R_MAXTOWNROLE) role = 0;
      }
    }
    if(team == T_MAFIA) {
      if(playerCount <= 8) {
        role = R_GODFATHER;
      } else if(playerCount <= 11) {
        if(mafiaPlayers[R_GODFATHER].sockd == 0) role = R_GODFATHER;
        else role = R_MAFIOSO;
      } else {
        if(mafiaPlayers[R_GODFATHER].sockd == 0) role = R_GODFATHER;
        else if(mafiaPlayers[R_MAFIOSO].sockd == 0) role = R_MAFIOSO;
        else role = (role %= 2) ? R_BLACKMAILER : R_CONSIGLIERE; //randomly chooses between the two
      }
      --mafiaCount;
    }
    if(team == T_NEUTRAL) {
      role %= R_MAXNEUTRALROLE + 1;
      --neutralCount;
      if(role == R_JESTER && neutralPlayers[R_EXECUTIONER].sockd != 0) {
        role = R_SERIALKILLER;
      } else if(role == R_EXECUTIONER && neutralPlayers[R_JESTER].sockd != 0) {
        role = R_SERIALKILLER;
      }
    }
    printf("Role: %d\n", role);
    allPlayers[i].team = team;
    allPlayers[i].role = role;
    if(team == T_TOWN) townPlayers[role] = allPlayers[i];
    if(team == T_MAFIA) mafiaPlayers[role] = allPlayers[i];
    if(team == T_NEUTRAL) neutralPlayers[role] = allPlayers[i];
  }
  for(int i = 0; i < playerCount; ++i) {
    printf("%s: %d %d\n", allPlayers[i].name, allPlayers[i].team, allPlayers[i].role);
  }
  return 0;
}
