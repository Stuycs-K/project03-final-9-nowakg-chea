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
//if id is -1, then the server sends the message
//else, it takes sockd of sender
void sendMessage(char* message, struct player allPlayers[], int id){
  char toClient[BUFFER_SIZE] = "[";
  if(id == -1) strcpy(toClient, "\033[32mserver");
  else {
    sprintf(toClient + 1, "%d] ", id);
    strcat(toClient, allPlayers[id].name);
  }
  strcat(toClient, ": ");
  strcat(toClient, message);
  if(id == -1) strcat(toClient, "\033[0m");
  for (int n = 0; allPlayers[n].sockd != 0; n++){
    write(allPlayers[n].sockd, toClient, BUFFER_SIZE);
  }
}

void timerSubserver(int toServer, int fromServer) {
  int phase = 0, time = 0, done = 0;
  while(!done) {
    read(fromServer, &phase, sizeof(int));
    switch(phase) {
      case GAMESTATE_DAY: time = 15; break;
      case GAMESTATE_DISCUSSION: time = 45; break;
      case GAMESTATE_VOTING: time = 30; break;
      case GAMESTATE_DEFENSE: time = 20; break;
      case GAMESTATE_JUDGEMENT: time = 20; break;
      case GAMESTATE_LASTWORDS: time = 7; break;
      case GAMESTATE_NIGHT: time = 37; break;
    }
    while(time--) {
      sleep(1);
      write(toServer, &time, sizeof(int));
    }
  }
}

void removePlayer(int sd, struct player* list) {
  int i = -1;
  while(list[++i].sockd != sd)
    if(i >= MAX_PLAYERS) printf("Missing sd in removePlayer\n");
  while(i < MAX_PLAYERS - 1 && list[i].sockd > 0) {
    list[i] = list[i+1];
    ++i;
  }
  list[i].sockd = 0;
}

//if there is a /vote or a /role then return 1 and get rid of the /vote in the string
int parsePlayerCommand(char *command){
  char delib[] = "/vote ";
  char delib2[] = "/role ";
  if( (strncmp(command, delib, strlen(delib)) != 0) || (strncmp(command, delib2, strlen(delib2)) != 0)){
    return 0;
  }

  //strlen delib and delib 2 are the same conviently enough
  for(int n = 0; n < strlen(delib); n++){
    command++;
  }
  //now command should point to the thing after /vote
  return 1;
}

int main() {
  signal(SIGCHLD, sighandler);


  //this needs to be done now so that later when the game starts it has the pipe working with the child server
  //subserver stuff
  int timerToMain[2];
  int mainToTimer[2];
  err(pipe(timerToMain), "pipe fail in beginning of main");
  err(pipe(mainToTimer), "pipe fail in beginning of main");
  pid_t subserver;
  subserver = fork();
  err(subserver, "fork fail in beginning");
  if(subserver == 0){
    close(timerToMain[PIPE_READ]);
    close(mainToTimer[PIPE_WRITE]);
    timerSubserver(timerToMain[PIPE_WRITE], mainToTimer[PIPE_READ]);
    return 0;
  }
  close(timerToMain[PIPE_WRITE]);
  close(mainToTimer[PIPE_READ]);


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

      write(newPlayer.sockd, "Enter a name:", 14);

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
  sendMessage("you have been connected to the server!", allPlayers, -1);






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
        else if(role % 2 == 0){
          role = R_BLACKMAILER;
        }  //randomly chooses between the two
        else {
          role = R_CONSIGLIERE;
        }
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

  sendMessage("Game: You're role and team is...", allPlayers, -1);

  for(int i = 0; i < playerCount; ++i) {
    printf("%s: %s %s\n", allPlayers[i].name, intToTeam(allPlayers[i].team), intToRole(allPlayers[i].role, allPlayers[i].team));
    write(allPlayers[i].sockd, intToRole(allPlayers[i].role, allPlayers[i].team), BUFFER_SIZE);
    write(allPlayers[i].sockd, intToTeam(allPlayers[i].team), BUFFER_SIZE);
  }

  sendMessage("Game: Hit enter to start!", allPlayers, -1);





  //BEGIN THE GAME

  int nextPhase = 0;
  int phase = GAMESTATE_DAY;

  int votingTries = 3;
  struct player *votedPlayer = NULL;

  int win = -1; //win will be equal to the team that wins so like T_MAFIA or T_TOWN

  printf("\n\nBEGINNING GAME!\n\n");

  while(win < 0){
    printf("new phase: %d\n", phase);
    //timer
    write(mainToTimer[PIPE_WRITE], &phase, sizeof(int));

    FD_ZERO(&read_fds);
    //add the sockd descriptor OF EVERY PLAYER and stdin to the set

    //voting and other server messages to tell players what is going on in the phase of the game
    switch(phase) {
              case GAMESTATE_DAY:
                sendMessage("Welcome to the Town of C-lem. If you are on the town team, you must vote to kill all the mafia members.  If you are the mafia you must kill all the town. If you are a neutral player you can win with either town or mafia but you have some other way to win. Have fun!", allPlayers, -1);
                break;
              case GAMESTATE_DISCUSSION:
                votingTries = 3;
                sendMessage("Discussion time!", allPlayers, -1);
                break;
              case GAMESTATE_VOTING:
                sendMessage("It is time to vote! You have %d tries left to vote to kill a player. Use /vote player_name to vote to put a player on trial.", allPlayers, -1);
                break;
              case GAMESTATE_DEFENSE:
                strcpy(buffer, votedPlayer->name);
                strcat(" is on trial. They will now state their case as to why they are not guilty!", buffer);
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_JUDGEMENT:
                strcpy(buffer, votedPlayer->name);
                strcat(" is on trial. Use /vote abstain, /vote guilty, and /vote innocent to vote whether they should be killed!", buffer);
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_LASTWORDS:
                strcpy(buffer, votedPlayer->name);
                strcat(" will now their last words.", buffer);
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_NIGHT:
                sendMessage("Talk to other mafia players and conspire to eliminate the town! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", mafiaPlayers, -1);
                sendMessage("Good luck! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", townPlayers, -1);
                sendMessage("Good luck! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", neutralPlayers, -1);
                break;
            }

    while(!nextPhase) {
      //add the pipe file descriptor
      FD_SET(timerToMain[PIPE_READ], &read_fds);

      for(int n = 0; n < playerCount; n++){
        //printf("FD_SETing %s\n", allPlayers[n].name);
        FD_SET(allPlayers[n].sockd, &read_fds);
      }

      int i = select(allPlayers[playerCount-1].sockd + 1, &read_fds, NULL, NULL, NULL);
      err(i, "server select/socket error in main game loop");


      if(FD_ISSET(timerToMain[PIPE_READ], &read_fds)){
        int time;
        read(timerToMain[PIPE_READ], &time, sizeof(int));
        if(time == 0) {
          nextPhase = 1;
          continue;
        }
        if(time == 120 || time == 60 || time == 30 || time == 15 || time == 10 || time == 5 || time == 4 || time == 3 || time == 2|| time == 1) printf("%ds left\n", time);
        continue;
      }

      for(int n = 0; n < playerCount; n++){
        if(FD_ISSET(allPlayers[n].sockd, &read_fds) && phase != GAMESTATE_DEFENSE && phase != GAMESTATE_LASTWORDS){
          int bytes = read(allPlayers[n].sockd, buffer, BUFFER_SIZE);
          err(bytes, "bad client read in game loop");
          if(bytes == 0) {
            char name[256];
            strcpy(name, allPlayers[n].name);
            int sd = allPlayers[n].sockd;
            if(allPlayers[n].team == T_TOWN) removePlayer(sd, townPlayers);
            if(allPlayers[n].team == T_MAFIA) removePlayer(sd, mafiaPlayers);
            if(allPlayers[n].team == T_NEUTRAL) removePlayer(sd, neutralPlayers);
            //printf("removing from all");
            removePlayer(sd, allPlayers);
            sprintf(buffer, "[%d] %s disconnected", n, name);
            sendMessage(buffer, allPlayers, -1);
            --playerCount;
            if(playerCount == 0) return 0;
            --n;
            continue;
          }
          printf("%s\n", buffer);

          //if there is a command like /vote playername or /role target
          if(parsePlayerCommand(buffer)){
            //buffer is now the target.

          }
          else{
            sendMessage(buffer, allPlayers, n);
          }

        }

      }

    }


    nextPhase = 0;
    phase++;
    if(phase == GAMESTATE_NIGHT + 1) phase = GAMESTATE_DISCUSSION;
    phase = 0;
  }





  return 0;
}
