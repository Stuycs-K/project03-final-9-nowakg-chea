#include "server.h"
#include "player.h"
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

// send a string to a list of players
void sendMessage(char* message, struct player allPlayers[]){
  for (int n = 0; allPlayers[n].sockd != 0; n++){
    write(allPlayers[n].sockd, message, BUFFER_SIZE);
  }
}


int main() {
  signal(SIGCHLD, sighandler);


  //this needs to be done now so that later when the game starts it has the pipe working with the child server
  //subserver stuff
  int subserverPipe[2];
  err(pipe(subserverPipe), "pipe fail in beginning of main");
  int gameState = GAMESTATE_PRE_GAME;
  pid_t subserver;
  subserver = fork();
  err(subserver, "fork fail in beginning");
  if(subserver == 0){
    printf("subserver writing, (pid: %d)\n", getpid());
    while(1){
      write( subserverPipe[PIPE_WRITE], GAMESTATE_DAY, sizeof(int));
    }
    // close(subserverPipe[PIPE_READ]);
    // close(subserverPipe[PIPE_WRITE]);
    exit(0);
  }



  //parent server
  printf("parent server pid: %d\n", getpid());
  printf("\nWaiting on people to join the game... (type /start to start)\n\n");

  int listen_socket = server_setup();

  char buffer[BUFFER_SIZE];
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
      fgets(buffer, sizeof(buffer), stdin);
      if(strcmp(buffer, "/start\n") == 0){
        // if(playerCount >= 7){
        //   printf("Starting the game!\n");
        //   joinPhase = 0;
        // }
        // else{
        //   printf("You need at least 7 players to start the game! There are only %d players in the lobby.\n", playerCount);
        // }
        //
        //REVERT BACK TO THIS ^^ ONCE WE GET A TESTER THAT CAN GENERATE 7 PLAYERS

        printf("Starting the game!\n");
        joinPhase = 0;
      }
    }

    if(FD_ISSET(listen_socket, &read_fds)){
      struct player newPlayer;
      newPlayer.sockd = server_tcp_handshake(listen_socket);

      write(newPlayer.sockd, "Enter a name:", BUFFER_SIZE);

      read(newPlayer.sockd, buffer, BUFFER_SIZE);
      strcpy(newPlayer.name, buffer);
      newPlayer.alive = 1;
      allPlayers[playerCount] = newPlayer;
      ++playerCount;
      printf("Player connected: %s\n", newPlayer.name );
      printf("Currently %d players\n", playerCount);
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

  printf("\nDISTRIBUTING ROLES!\n\n");
  int townCount = 6;
  int mafiaCount = 1;
  int neutralCount = 0;
  for(int i = 7; i < playerCount; ++i) {
    if(i % 3 == 1) ++townCount;
    if(i % 3 == 2) ++mafiaCount;
    if(i % 3 == 0) ++neutralCount;
  }
  //start giving roles to each player
  for(int i = 0; i < playerCount; ++i) {
    //decide the team
    //printf("Player: %d\n", i);
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
  //  printf("Team: %d\n", team);
    //decide the role
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
    //printf("Role: %d\n", role);
    //assign to lists
    allPlayers[i].team = team;
    allPlayers[i].role = role;
    if(team == T_TOWN) townPlayers[role] = allPlayers[i];
    if(team == T_MAFIA) mafiaPlayers[role] = allPlayers[i];
    if(team == T_NEUTRAL) neutralPlayers[role] = allPlayers[i];
  }

  sendMessage("GAME: You're role and team is...", allPlayers);

  for(int i = 0; i < playerCount; ++i) {
    printf("%s: %s %s\n", allPlayers[i].name, intToTeam(allPlayers[i].team), intToRole(allPlayers[i].role, allPlayers[i].team));
    write(allPlayers[i].sockd, intToRole(allPlayers[i].role, allPlayers[i].team), BUFFER_SIZE);
    write(allPlayers[i].sockd, intToTeam(allPlayers[i].team), BUFFER_SIZE);
  }






  //BEGIN THE GAME

  printf("\n\nBEGINNING GAME!\n");
  int win = -1; //will be equal to the team that wins so like T_MAFIA or T_TOWN

  while(win < 0){
    FD_ZERO(&read_fds);
    //add the sockd descriptor OF EVERY PLAYER and stdin to the set

    for(int n = 0; n < playerCount; n++){
      printf("FD_SETing %s\n", allPlayers[n].name);
      FD_SET(allPlayers[n].sockd, &read_fds);
    }

    //add the pipe file descriptor
    //FD_SET(subserverPipe[PIPE_READ], &read_fds);

    //get the pipe read and listen_socket to both be listened to

    //THIS IS BLOCKING THE REST OF SERVER
    int i = select(listen_socket+1, &read_fds, NULL, NULL, NULL);
    err(i, "server select/socket error in main game loop");


    if(gameState == GAMESTATE_DAY){
      sendMessage("\n\nGAME: It is now daytime! Talk amongst the townfolk.\n\n", allPlayers);
    }

    for (int n = 0; n < playerCount; n++){
      if(FD_ISSET(allPlayers[n].sockd, &read_fds)){
        read(listen_socket, buffer, BUFFER_SIZE);
        sendMessage(buffer, allPlayers);
      }
    }


    if(FD_ISSET(subserverPipe[PIPE_READ], &read_fds)){
      gameState = subserverPipe[PIPE_READ];
      printf("gameState is now: %d\n", gameState);
    }
  }





  return 0;
}
