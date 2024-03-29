#include "server.h"
#include "role.h"
#include "input.h"

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
  for (int n = 0; n < MAX_PLAYERS; n++){
    if(allPlayers[n].sockd > 0) write(allPlayers[n].sockd, toClient, BUFFER_SIZE);
  }
}

void colorMessage(char* message, struct player allPlayers[], int id, int color){
  char toClient[BUFFER_SIZE];
  sprintf(toClient, "\033[%dm[%d] ", color, id);
  strcat(toClient, allPlayers[id].name);
  strcat(toClient, ": ");
  strcat(toClient, message);
  strcat(toClient, "\033[0m");
  for (int n = 0; n < MAX_PLAYERS; n++){
    if(allPlayers[n].sockd > 0) write(allPlayers[n].sockd, toClient, BUFFER_SIZE);
  }
}

//takes SOCKET DESCRIPTOR of target and ID NUMBER of sender
void singleMessage(char* message, int targetSD, int senderID, char* senderName) {
  char toClient[BUFFER_SIZE] = "\033[90mWhisper from [";
  if(senderID == -1) strcpy(toClient, "\033[32mserver");
  else {
    sprintf(toClient + strlen("\033[90mWhisper from ["), "%d] ", senderID);
    strcat(toClient, senderName);
  }
  strcat(toClient, ": ");
  strcat(toClient, message);
  strcat(toClient, "\033[0m");
  write(targetSD, toClient, BUFFER_SIZE);
}

void timerSubserver(int toServer, int fromServer) {
  int phase = 0, time = 0, done = 0;
  while(!done) {
    read(fromServer, &phase, sizeof(int));
    printf("phase (time): %d \n", phase);
    switch(phase) {
      case GAMESTATE_DAY: time = 15; break;
      case GAMESTATE_DISCUSSION: time = 30; break;
      case GAMESTATE_VOTING: time = 30; break;
      case GAMESTATE_DEFENSE: time = 30; break; //originally 20
      case GAMESTATE_JUDGEMENT: time = 20; break;
      case GAMESTATE_VOTE_COUNTING: time = 1; break;
      case GAMESTATE_LASTWORDS: time = 7; break;
      case GAMESTATE_KILL_VOTED: time = 5; break;
      case GAMESTATE_NIGHT: time = 45; break; //orignally 37
      case GAMESTATE_RUN_NIGHT: time = 5; break;
    }
    while(time--) {
      sleep(1);
      write(toServer, &time, sizeof(int));
    }
  }
}

//if there is delib in command increment the pointer to go past the delib
char* parsePlayerCommand(char *command, char* delib){
  if( strncmp(command, delib, strlen(delib)) == 0){
    //after this command should point to the char after /vote
    for(int n = 0; n < strlen(delib); n++){
      command++;
    }
  }
  return command;
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

  char* buffer = calloc(sizeof(char), BUFFER_SIZE);
  struct player* allPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* townPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* mafiaPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* neutralPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* alivePlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* deadPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* votedPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
  struct player* dyingPlayers = calloc(sizeof(struct player), MAX_PLAYERS);

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
      newPlayer.alive = TRUE;

      newPlayer.votesForTrial = 0;
      newPlayer.votedFor = -1;
      newPlayer.whatVote = VOTE_ABSTAIN;

      allPlayers[playerCount] = newPlayer;
      alivePlayers[playerCount] = newPlayer;
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

    if(i == 0) {
      allPlayers[i].team = T_MAFIA;
      allPlayers[i].role = R_GODFATHER;
      alivePlayers[i].team = T_MAFIA;
      alivePlayers[i].role = R_GODFATHER;
      mafiaPlayers[R_GODFATHER] = allPlayers[i];
      --mafiaCount;
      continue;
    }
    if(i == 1) {
      allPlayers[i].team = T_TOWN;
      allPlayers[i].role = R_VETERAN;
      alivePlayers[i].team = T_TOWN;
      alivePlayers[i].role = R_VETERAN;
      townPlayers[R_VETERAN] = allPlayers[i];
      --townCount;
      continue;
    }
    if(i == 2) {
      allPlayers[i].team = T_NEUTRAL;
      allPlayers[i].role = R_SERIALKILLER;
      alivePlayers[i].team = T_NEUTRAL;
      alivePlayers[i].role = R_SERIALKILLER;
      neutralPlayers[R_SERIALKILLER] = allPlayers[i];
      --neutralCount;
      continue;
    }

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
    alivePlayers[i].team = team;
    alivePlayers[i].role = role;
    if(team == T_TOWN) townPlayers[role] = allPlayers[i];
    if(team == T_MAFIA) mafiaPlayers[role] = allPlayers[i];
    if(team == T_NEUTRAL) neutralPlayers[role] = allPlayers[i];
  }



  sendMessage("Game: Your role and team is...", allPlayers, -1);

  int townAlive = 0;
  int mafiaAlive = 0;
  int neutralAlive = 0;
  int serialKillerAlive = 0;
  //used for win conditions LATER

  // used for later when dead people need to chat to the medium
  int mediumSD = -1;

  //for when jailor needs to jail his prisoner
  int jailorID = -1;

  //for reading whsipers
  int blackmailerSD = -1;

  int revivedID = -1;

  for(int i = 0; i < playerCount; ++i) {
    printf("%s: %s %s\n", allPlayers[i].name, intToTeam(allPlayers[i].team), intToRole(allPlayers[i].role, allPlayers[i].team));
    write(allPlayers[i].sockd, intToRole(allPlayers[i].role, allPlayers[i].team), BUFFER_SIZE);
    write(allPlayers[i].sockd, intToTeam(allPlayers[i].team), BUFFER_SIZE);

    allPlayers[i].defense = NO_DEFENSE;
    allPlayers[i].attack = NO_ATTACK;
    allPlayers[i].addedDefense = 0;
    allPlayers[i].addedAttack = 0;
    allPlayers[i].rolePriority = -1;
    allPlayers[i].visiting = -1;
    allPlayers[i].blackmailed = FALSE;
    allPlayers[jailorID].jailorPrisonerID = -1;

    for(int n = 0; n < MAX_PLAYERS; n++){
      allPlayers[i].visitorsID[n] = -1;
    }


    if(allPlayers[i].team == T_TOWN){
      townAlive++;
      switch (allPlayers[i].role){
        case R_VETERAN:
          allPlayers[i].attack = POWERFUL_ATTACK;
          allPlayers[i].rolePriority = 1;
          allPlayers[i].veteranAlert = FALSE;
          allPlayers[i].veteranAlertCount = 3;
          break;
        case R_MEDIUM:
          mediumSD = allPlayers[i].sockd; //for later that medium can talk to dead at night
          break;
        case R_RETROBUTIONIST:
          allPlayers[i].rolePriority = 1;
          allPlayers[i].veteranAlertCount = 1;
          break;
        case R_DOCTOR:
          allPlayers[i].rolePriority = 3;
          break;
        case R_LOOKOUT:
          allPlayers[i].rolePriority = 4;
          break;
        case R_SHERIFF:
          allPlayers[i].rolePriority = 4;
          break;
        case R_JAILOR:
          jailorID = i; //for later when jailor jails people
          allPlayers[i].attack = UNSTOPPABLE_ATTACK;
          allPlayers[i].rolePriority = 5;
          allPlayers[i].jailorPrisonerID = -1;
          break;
        case R_VIGILANTE:
          allPlayers[i].attack = BASIC_ATTACK;
          allPlayers[i].rolePriority = 5;
          allPlayers[i].vigilanteBullets = 3;
          break;
        case R_MAYOR:
          allPlayers[i].rolePriority = 1;
          break;
      }
    }
    if(allPlayers[i].team == T_MAFIA){
      mafiaAlive++;

      switch (allPlayers[i].role){
        case R_GODFATHER:
          allPlayers[i].defense = BASIC_DEFENSE;
          allPlayers[i].attack = BASIC_ATTACK;
          allPlayers[i].rolePriority = 5;
          break;
        case R_MAFIOSO:
          allPlayers[i].attack = BASIC_ATTACK;
          allPlayers[i].rolePriority = 5;
          break;
        case R_CONSIGLIERE:
          allPlayers[i].rolePriority = 4;
          break;
        case R_BLACKMAILER:
          allPlayers[i].rolePriority = 3;
          blackmailerSD = allPlayers[i].sockd; //for later when blackmailing people and reading whsipers
          break;
      }
    }
    if(allPlayers[i].team == T_NEUTRAL){
      neutralAlive++;

      switch (allPlayers[i].role){
        case R_EXECUTIONER:
          allPlayers[i].defense = BASIC_DEFENSE;
          break;
        case R_SERIALKILLER:
          allPlayers[i].defense = BASIC_DEFENSE;
          allPlayers[i].attack = BASIC_ATTACK;
          serialKillerAlive = TRUE;
          break;
        }



    }
  }

  sendMessage("Your team:", mafiaPlayers, -1);
  for(int i = 0; i < MAX_PLAYERS; ++i) {
    if(allPlayers[i].sockd > 0 && allPlayers[i].team == T_MAFIA) {
      sprintf(buffer, "[%d] %s - %s", i, allPlayers[i].name, intToRole(allPlayers[i].role, allPlayers[i].team));
      sendMessage(buffer, mafiaPlayers, -1);
    }
  }

  int executionerTarget = -1;
  if(neutralPlayers[R_EXECUTIONER].sockd > 0) {
    int r = open("/dev/random", O_RDONLY);
    int target;
    read(r, &target, sizeof(int));
    close(r);
    if(target < 0) target *= -1;
    target %= playerCount;
    if(allPlayers[target].team == T_NEUTRAL && allPlayers[target].role == R_EXECUTIONER) --target;
    if(target == -1) target += 2; //if executioner is p0, make it p1
    printf("Executioner target: %d\n", target);
    sprintf(buffer, "Your target: [%d] %s", target, allPlayers[target].name);
    singleMessage(buffer, neutralPlayers[R_EXECUTIONER].sockd, -1, NULL);
    executionerTarget = target;
  }

  int jesterWin = 0, executionerWin = 0;

  //BEGIN THE GAME

  int nextPhase = 0;
  int phase = GAMESTATE_DAY;
  struct player* votedPlayersList = votedPlayers;

  int votingTries = 3;
  struct player* votedPlayer = NULL;
  int guiltyVotes = 0, innoVotes = 0, abstVotes = 0; //will be used in the GAMESTATE_JUDGEMENT phase

  int mayorReveal = FALSE;
  //for mayor later

  int win = -1; //win will be equal to the team that wins so like T_MAFIA or T_TOWN

  printf("\n\nBEGINNING GAME!\n\n");
  while(win < 0){
    printf("new phase: %d\n", phase);
    if(phase == GAMESTATE_DISCUSSION) {

      if(revivedID > -1) {
        alivePlayers[revivedID] = deadPlayers[revivedID];
        deadPlayers[revivedID].sockd = 0;
        sprintf(buffer, "[%d] %s was revived last night!", revivedID, allPlayers[revivedID].name);
        sendMessage(buffer, allPlayers, -1);
        singleMessage("You have been revived!", allPlayers[revivedID].sockd, -1, NULL);
        if(allPlayers[revivedID].team == T_TOWN) {
          townPlayers[allPlayers[revivedID].role].alive = TRUE;
          ++townAlive;
        }
        if(allPlayers[revivedID].team == T_MAFIA) {
          mafiaPlayers[allPlayers[revivedID].role].alive = TRUE;
          ++mafiaAlive;
        }
        if(allPlayers[revivedID].team == T_NEUTRAL) {
          neutralPlayers[allPlayers[revivedID].role].alive = TRUE;
          ++neutralAlive;
        }
        ++playerCount;
        revivedID = -1;
      }

        //move dying players to dead players and reset everyone's addedAttack and addedDefense
      for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(dyingPlayers[i].sockd > 0) {
          allPlayers[i].alive = FALSE;
          deadPlayers[i] = dyingPlayers[i];
          if(deadPlayers[i].team == T_TOWN) {
            --townAlive;
            if(allPlayers[i].role == R_MEDIUM){
              mediumSD = -1;
            }
            if(townAlive == 0) {
              win = T_MAFIA;
              break;
            }
          }
          if(deadPlayers[i].team == T_MAFIA) {
            --mafiaAlive;
            if(mafiaAlive == 0) {
              win = T_TOWN;
              break;
            }
          }
          if(deadPlayers[i].team == T_NEUTRAL) {
            --neutralAlive;
            if(deadPlayers[i].role == R_SERIALKILLER){
              serialKillerAlive = FALSE;
            }
          }
          sprintf(buffer, "%s died last night. Their role was %s.", deadPlayers[i].name, intToRole(deadPlayers[i].role, deadPlayers[i].team));
          sendMessage(buffer, allPlayers, -1);
          singleMessage("You have been killed! You can now talk with other dead players.", deadPlayers[i].sockd, -1, NULL);
          --playerCount;
          sleep(2);
        }
      }
      //free(dyingPlayers);
      dyingPlayers = calloc(sizeof(struct player), MAX_PLAYERS);
    }

    //win logic
    if(mafiaAlive == 0 && serialKillerAlive == FALSE){
      win = T_TOWN;
      break;
    }
    if(townAlive == 0 && serialKillerAlive == FALSE){
      win = T_MAFIA;
      break;
    }
    /*
    if(neutralAlive == 0){
      win = T_NEUTRAL;
    }
    */

    FD_ZERO(&read_fds);

    switch(phase) {
              case GAMESTATE_DAY:
                sendMessage("Welcome to the Town of C-lem. If you are on the town team, you must vote to kill all the mafia members.  If you are the mafia you must kill all the town. If you are a neutral player you can win with either town or mafia but you have some other way to win. Have fun!", allPlayers, -1);
                break;
              case GAMESTATE_DISCUSSION:
                votingTries = 3;
                sendMessage("Discussion time!", allPlayers, -1);
                break;
              case GAMESTATE_VOTING:
                if(votingTries <= 0){
                  sendMessage("You have run out of chances to vote to kill a player. NIGHT APPROCHES!", allPlayers, -1);
                  phase = GAMESTATE_NIGHT;
                  continue;
                }
                votingTries--;
                votedPlayersList = votedPlayers;
                //WILL NEED TO BE CHANGED TO ALIVE PLAYERS LATER BUT THIS IS JUST FOR TESTING PURPOSES
                //resets amount of votes for this player to stand trial
                for(int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd > 0) {
                    allPlayers[n].votesForTrial = 0;
                    allPlayers[n].votedFor = -1;
                  }
                }
                sprintf(buffer,"It is time to vote! You have %d tries left to vote to kill a player. Use /vote player_name to vote to put a player on trial.",votingTries);
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_DEFENSE:
                 //WILL NEED TO CHANGE TO ALIVE PLAYRES LATER BUT THIS IS FINE FORE NOW
                sendMessage("Counting votes...", allPlayers, -1);
                int highestVote = 0;

                for(int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd > 0){
                    allPlayers[allPlayers[n].votedFor].votesForTrial++;
                    if(allPlayers[n].role == R_MAYOR && mayorReveal == TRUE){
                      allPlayers[allPlayers[n].votedFor].votesForTrial += 2;
                    }
                  }
                }


                for (int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd > 0 && allPlayers[n].votesForTrial > highestVote){
                    highestVote = allPlayers[n].votesForTrial;
                    votedPlayer = &allPlayers[n];
                    printf("voted player: %s\n", votedPlayer->name);
                  }
                }
                printf("highest: %d, playerCount: %d\n", highestVote, playerCount);

                if(highestVote <= playerCount / 2){
                  //might be a probelm cuz this continue doesn't go out of the while loop
                  votedPlayer = NULL;
                  sendMessage("The town has voted, but there aren't enough votes to put someone on trial. Night approaches!", allPlayers, -1);
                  //nextPhase = 0;
                  phase = GAMESTATE_NIGHT;
                  continue;
                }
                else{
                  sprintf(buffer, "The town has voted! %s will now be put on trial.", votedPlayer->name);
                  sendMessage(buffer, allPlayers, -1);
                }




                strcpy(buffer, votedPlayer->name);
                printf("phase %d\n", phase);
                strcat(buffer, " is on trial. They will now state their case as to why they are not guilty!");
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_JUDGEMENT:
                strcpy(buffer, votedPlayer->name);
                strcat(buffer, " is on trial. Use /vote abstain, /vote guilty, and /vote innocent to vote whether they should be killed!");
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_VOTE_COUNTING:
              //count the votes of all players
                for(int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd > 0){
                    if(allPlayers[n].whatVote == VOTE_ABSTAIN){
                      abstVotes++;
                      if(allPlayers[n].role == R_MAYOR && mayorReveal == TRUE){
                        abstVotes = abstVotes + 2;
                      }
                    }
                    if(allPlayers[n].whatVote == VOTE_INNOCENT){
                      innoVotes++;
                      if(allPlayers[n].role == R_MAYOR && mayorReveal == TRUE){
                        innoVotes = innoVotes + 2;
                      }
                    }
                    if(allPlayers[n].whatVote == VOTE_GUILTY){
                      guiltyVotes++;
                      if(allPlayers[n].role == R_MAYOR && mayorReveal == TRUE){
                        guiltyVotes = guiltyVotes + 2;
                      }
                    }
                  }
                }
                if(guiltyVotes <= innoVotes){
                  char noVoteMessage[BUFFER_SIZE];
                  sprintf(noVoteMessage, "%s had not enough votes to be killed. You now have %d tries left to vote a player. ", votedPlayer->name, votingTries);
                  sendMessage(noVoteMessage, allPlayers, -1);
                  phase = GAMESTATE_VOTING;
                  votedPlayer = NULL;
                  continue;
                }
                abstVotes = 0;
                innoVotes = 0;
                guiltyVotes = 0;

                //reset player's votes
                for(int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd > 0){
                    allPlayers[n].whatVote = VOTE_ABSTAIN;
                  }
                }
                phase = GAMESTATE_LASTWORDS;
                continue;
                break;

              case GAMESTATE_LASTWORDS:
                strcpy(buffer, votedPlayer->name);
                strcat(buffer, " will now say their last words.");
                sendMessage(buffer, allPlayers, -1);
                break;

              case GAMESTATE_KILL_VOTED:
                if(votedPlayer != NULL && votedPlayer->sockd != 0){
                  sprintf(buffer, "%s has been killed. (insert dying animation).\n Their team was: %s and their role was: %s.", votedPlayer->name, intToTeam(votedPlayer->team), intToRole(votedPlayer->role, votedPlayer->team));
                  sendMessage(buffer, allPlayers, -1);

                  if(votedPlayer->role == R_JESTER){
                    singleMessage("Congratulations! You won. Stick around and you will be able to kill somebody at night!", votedPlayer->sockd, -1, NULL);
                    jesterWin = 1;
                  }

                  if(votedPlayer->sockd == allPlayers[executionerTarget].sockd) {
                    sendMessage("The executioner's target has been voted out!", allPlayers, -1);
                    singleMessage("Congratulations! You won. Stick around and you will be able to side with either town or mafia but you win either way.", neutralPlayers[R_EXECUTIONER].sockd, -1, NULL);
                    executionerWin = 1;
                  }

                  //KILL THE GUILTY PLAYER!!!
                  //KILL PLAYER HERE
                  movePlayer(votedPlayer->sockd, alivePlayers, deadPlayers);
                  singleMessage("You have been killed! You can now talk with other dead players.", votedPlayer->sockd, -1, NULL);
                  --playerCount;
                  if(votedPlayer->team == T_TOWN) {
                    --townAlive;
                    if(townAlive == 0) {
                      win = T_MAFIA;
                      break;
                    }
                  }
                  if(votedPlayer->team == T_MAFIA) {
                    --mafiaAlive;
                    if(mafiaAlive == 0) {
                      win = T_TOWN;
                      break;
                    }
                  }
                  if(votedPlayer->team == T_NEUTRAL) {
                    --neutralAlive;
                    if(votedPlayer->role == R_SERIALKILLER){
                      serialKillerAlive = FALSE;
                    }
                  }
                  votedPlayer = NULL;
                  phase = GAMESTATE_NIGHT;
                  continue;
                }
                break;

              case GAMESTATE_NIGHT:
                //get rid of blackmailed players
                for(int n = 0; n < MAX_PLAYERS; n++){
                  if(allPlayers[n].sockd <= 0){
                    continue;
                  }
                  allPlayers[n].blackmailed = FALSE;

                  //throw the jailor's prisoner in jail
                  if(allPlayers[n].role == R_JAILOR && allPlayers[allPlayers[jailorID].jailorPrisonerID].sockd > 0 && allPlayers[allPlayers[jailorID].jailorPrisonerID].alive == TRUE){
                    singleMessage("You've been hauled to jail! The jailor is now going to interrogate you.", allPlayers[allPlayers[jailorID].jailorPrisonerID].sockd, -1, "Jailor");
                    allPlayers[allPlayers[jailorID].jailorPrisonerID].addedDefense = POWERFUL_DEFENSE;
                    //printf("%s: defense: %d\n",allPlayers[allPlayers[jailorID].jailorPrisonerID].name,allPlayers[allPlayers[jailorID].jailorPrisonerID].addedDefense);
                    continue;
                  }
                }




                sendMessage("Talk to other mafia players and conspire to eliminate the town! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", mafiaPlayers, -1);
                sendMessage("Good luck! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", townPlayers, -1);
                sendMessage("Good luck! Use /role target to do your role's action. If you have a role that requires you to target yourself do /role your-name.", neutralPlayers, -1);
                break;



                  //all the role actions should go here.
              case GAMESTATE_RUN_NIGHT:
                //PLAYER VISITS
                //this will make it so that players with higher priority visit people first
                for(int prio = 1; prio < 7; prio++){
                  for(int n = 0; n < MAX_PLAYERS; n++){
                    if(allPlayers[n].sockd <= 0 || allPlayers[n].rolePriority != prio){
                      continue;
                    }
                    printf("WE ARE NOW FIGURING OUT: %s PLAYER STUF THAT HAPPENED AT NIGHT\n", allPlayers[n].name);
                    printf("NOW PARSING visiting: ID: %d \n", allPlayers[n].visiting);

                    //     set the array of the player being visited to the visiting player
                    if(allPlayers[n].visiting > -1){
                      for(int i = 0; i < MAX_PLAYERS; i++){
                        if(allPlayers[allPlayers[n].visiting].visitorsID[i] == -1){
                          allPlayers[allPlayers[n].visiting].visitorsID[i] = n;
                          break;
                        }
                      }
                    }
                  }
                }


                //PLAYER DEFENSE AND ATTACK CALUCLATIONS / INVESTIGATES
                for(int n = 0; n < MAX_PLAYERS; n++){
                  for(int i = 0; i < MAX_PLAYERS; i++){
                    if(allPlayers[n].sockd <= 0 || allPlayers[n].visitorsID[i] < 0){
                      continue;
                    }
                    printf("%s, visited by: %s\n", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].name);

                    // allPlayers[allPlayers[n].visitorsID[i]] is visitor
                    char visitorRelay[BUFFER_SIZE] = "";

                    //throw the jailor's prisoner in jail they can't do their role action if they are in jail
                    if(i == allPlayers[jailorID].jailorPrisonerID){
                      sprintf(visitorRelay, "You've been hauled to jail! You couldn't preform your action tonight.");
                      singleMessage(visitorRelay, allPlayers[i].sockd, -1, NULL);
                      allPlayers[allPlayers[jailorID].jailorPrisonerID].addedDefense = POWERFUL_DEFENSE;
                      //reset jailorPrisonerID
                      allPlayers[jailorID].jailorPrisonerID = -1;
                      continue;
                    }

                    if(allPlayers[allPlayers[n].visitorsID[i]].team == T_TOWN){
                      switch ( allPlayers[allPlayers[n].visitorsID[i]].role ){
                        case R_DOCTOR:
                          allPlayers[n].addedDefense = POWERFUL_DEFENSE;
                          sprintf(visitorRelay, "You gave emergency medical aid to %s!", allPlayers[n].name);
                          break;

                        case R_LOOKOUT:
                          int foundSomeone = FALSE;
                          sprintf(visitorRelay,"You checked out %s's house and found...\n", allPlayers[n].name);

                          for(int a = 0; a < MAX_PLAYERS; a++){
                            if(allPlayers[n].sockd <= 0 || allPlayers[n].visitorsID[a] < 0){
                              continue;
                            }
                            foundSomeone = TRUE;
                            strcat(visitorRelay, allPlayers[allPlayers[n].visitorsID[a]].name );
                            strcat(visitorRelay, " \n");
                          }
                          if(foundSomeone){
                            strcat(visitorRelay, "lurking around!");
                          }
                          else{
                            strcat(visitorRelay, "nobody!");
                          }
                          break;

                        case R_SHERIFF:
                          if(  (allPlayers[n].team == T_MAFIA && allPlayers[n].role != R_GODFATHER)  || allPlayers[n].role == R_SERIALKILLER ){
                            sprintf(visitorRelay, "Your target %s seems to either be a criminal or someone who has been framed!", allPlayers[n].name);
                          }
                          else{
                            sprintf(visitorRelay, "Your target %s seems to either be innocent or really great at hiding secrets!", allPlayers[n].name);
                          }
                          break;

                        case R_JAILOR:
                          allPlayers[n].addedDefense = POWERFUL_DEFENSE;
                          break;

                        case R_VIGILANTE:

                          if(allPlayers[allPlayers[n].visitorsID[i]].vigilanteBullets <= 0){
                            sprintf(visitorRelay, "You went to %s's home but you had no bullets left to shoot them with!", allPlayers[n].name);
                            continue;
                          }

                          allPlayers[allPlayers[n].visitorsID[i]].vigilanteBullets--;
                          if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                            //KILL PLAYER HERE
                            movePlayer(allPlayers[n].sockd, alivePlayers, dyingPlayers);
                            sprintf(visitorRelay, "You killed %s! You have %d bullets left in your gun to kill other players.", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].vigilanteBullets);

                            if(allPlayers[n].team == T_TOWN){
                              //KILL PLAYER HERE
                              movePlayer(allPlayers[n].sockd, alivePlayers, dyingPlayers);
                              strcat(visitorRelay, "You killed yourself out of regret for killing a fellow town member! You are dead.");
                            }

                          }
                          else{
                            sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, their defense: %d, their added defense: %d. You have %d bullets left in your gun to kill other players. ", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense , allPlayers[allPlayers[n].visitorsID[i]].vigilanteBullets );
                          }
                          break;
                        }
                    }
                    else if(allPlayers[allPlayers[n].visitorsID[i]].team == T_MAFIA){
                        switch ( allPlayers[allPlayers[n].visitorsID[i]].role ){

                          case R_GODFATHER:
                            if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                              //KILL PLAYER HERE
                              movePlayer(allPlayers[n].sockd, alivePlayers, dyingPlayers);
                              sprintf(visitorRelay, "You killed %s!", allPlayers[n].name);
                            }
                            else{
                              sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, theikr defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                            }
                            break;
                          case R_MAFIOSO:
                            if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                              //KILL PLAYER HERE
                              movePlayer(allPlayers[n].sockd, alivePlayers, dyingPlayers);
                              sprintf(visitorRelay, "You killed %s!", allPlayers[n].name);
                            }
                            else{
                              sprintf(visitorRelay, "You tried to kill %s but their defence was too high!", allPlayers[n].name);
                            }
                            break;
                          case R_CONSIGLIERE:
                            sprintf(visitorRelay, "Your target %s is a %s", allPlayers[n].name, intToRole(allPlayers[n].team, allPlayers[n].role));
                            break;
                          case R_BLACKMAILER:
                            sprintf(visitorRelay, "You have blackmailed %s, and they cannot talk for the day!", allPlayers[n].name);
                            singleMessage("You have been blackmailed. You cannot talk for the entire day!", allPlayers[n].sockd,-1,NULL);
                            allPlayers[n].blackmailed = TRUE;
                            break;
                        }
                      }
                      else if(allPlayers[allPlayers[n].visitorsID[i]].team == T_NEUTRAL){
                          switch ( allPlayers[allPlayers[n].visitorsID[i]].role ){
                            case R_SERIALKILLER:
                              if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                                //KILL PLAYER HERE
                                movePlayer(allPlayers[n].sockd, alivePlayers, dyingPlayers);
                                sprintf(visitorRelay, "You killed %s!", allPlayers[n].name);
                              }
                              else{
                                sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, theikr defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                              }
                              break;
                          }
                        }

                      if(allPlayers[n].role == R_VETERAN && allPlayers[n].veteranAlert == TRUE){
                        if(allPlayers[n].attack > allPlayers[allPlayers[n].visitorsID[i]].defense && allPlayers[n].attack > allPlayers[allPlayers[n].visitorsID[i]].addedDefense){
                          //KILL PLAYER HERE
                          movePlayer(allPlayers[allPlayers[n].visitorsID[i]].sockd, alivePlayers, dyingPlayers);
                          char vetMessage[BUFFER_SIZE];
                          sprintf(vetMessage, "You killed %s while alert.", allPlayers[allPlayers[n].visitorsID[i]].name);
                          singleMessage(vetMessage, allPlayers[n].sockd, -1, NULL);
                          sprintf(visitorRelay, "You were killed by an alert veteran (%s)!", allPlayers[n].name);
                        }
                        else{
                            sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, their defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                        }
                      }

                      if(allPlayers[n].role == R_JAILOR && allPlayers[n].executePrisoner == TRUE){
                        char jailorMessage[BUFFER_SIZE];
                        if(allPlayers[n].attack > allPlayers[allPlayers[n].visitorsID[i]].defense && allPlayers[n].attack > allPlayers[allPlayers[n].visitorsID[i]].addedDefense){
                          //KILL PLAYER HERE
                          movePlayer(allPlayers[allPlayers[n].visitorsID[i]].sockd, alivePlayers, dyingPlayers);
                          sprintf(jailorMessage, "You executed %s! They are dead.", allPlayers[allPlayers[n].visitorsID[i]].name);
                          singleMessage(jailorMessage, allPlayers[n].sockd, -1, NULL);
                          sprintf(visitorRelay, "You were killed by the jailor!");
                        }
                        else{
                          sprintf(jailorMessage, "You tried to kill %s but their defence was too high! your attack %d, their defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                          singleMessage(jailorMessage, allPlayers[n].sockd, -1, NULL);
                        }
                      }
                      singleMessage(visitorRelay, allPlayers[allPlayers[n].visitorsID[i]].sockd, -1, NULL);
                    }
                  }


                for (int i = 0; i < MAX_PLAYERS; i++){
                  if(allPlayers[i].sockd > 0){
                    allPlayers[i].addedDefense = 0;
                    allPlayers[i].addedAttack = 0;
                    allPlayers[i].visiting = -1;
                    for(int n = 0; n < MAX_PLAYERS; n++){
                      allPlayers[i].visitorsID[n] = -1;
                    }
                  }
                }
                phase = GAMESTATE_DISCUSSION;
                continue;
                break;
            }

    //timer
    write(mainToTimer[PIPE_WRITE], &phase, sizeof(int));
    int godfatherOverride = 0;
    while(!nextPhase) {
      FD_ZERO(&read_fds);
      //add the pipe file descriptor
      FD_SET(timerToMain[PIPE_READ], &read_fds);

      int maxSD = -1;

      for(int n = 0; n < MAX_PLAYERS; n++){
        //printf("FD_SETing %s\n", allPlayers[n].name);
        if(allPlayers[n].sockd > 0) {
          FD_SET(allPlayers[n].sockd, &read_fds);
          if(allPlayers[n].sockd > maxSD) maxSD = allPlayers[n].sockd;
        }
      }

      int i = select(maxSD + 1, &read_fds, NULL, NULL, NULL);
      err(i, "server select/socket error in main game loop");


      if(FD_ISSET(timerToMain[PIPE_READ], &read_fds)){
        int time;
        read(timerToMain[PIPE_READ], &time, sizeof(int));
        if(time == 0) {
          nextPhase = 1;
          continue;
        }
        if(time == 120 || time == 60 || time == 30 || time == 10 || time == 3 || time == 2|| time == 1) {
          char timeMsg[BUFFER_SIZE];
          sprintf(timeMsg, "%ds left", time);
          printf("%s", timeMsg);
          sendMessage(timeMsg, allPlayers, -1);
        }
        continue;
      }

      for(int n = 0; n < MAX_PLAYERS; n++){
        char vote[BUFFER_SIZE];
        //if(allPlayers[n].sockd > 0) printf("%d %d %d\n", allPlayers[n].sockd, alivePlayers[n].sockd, deadPlayers[n].sockd);
        if(alivePlayers[n].sockd > 0 && FD_ISSET(alivePlayers[n].sockd, &read_fds)){
          int bytes = read(allPlayers[n].sockd, buffer, BUFFER_SIZE);
          err(bytes, "bad client read in game loop");
          if(bytes == 0) {
            char name[256];
            strcpy(name, allPlayers[n].name);
            printf("%s disconnected\n", name);
            int sd = allPlayers[n].sockd;
            if(allPlayers[n].team == T_TOWN) {
              --townAlive;
              if(allPlayers[n].role == R_MEDIUM){
                mediumSD = -1;
              }
              if(townAlive == 0) {
                win = T_MAFIA;
                break;
              }
              movePlayer(sd, townPlayers, NULL);
            }
            if(allPlayers[n].team == T_MAFIA) {
              --mafiaAlive;
              if(mafiaAlive == 0) {
                win = T_TOWN;
                break;
              }
              movePlayer(sd, mafiaPlayers, NULL);
            }
            if(allPlayers[n].team == T_NEUTRAL) {
              --neutralAlive;
              movePlayer(sd, neutralPlayers, NULL);
            }
            //printf("removing from all");
            movePlayer(sd, allPlayers, NULL);
            sprintf(buffer, "[%d] %s disconnected", n, name);
            sendMessage(buffer, allPlayers, -1);
            --playerCount;
            if(playerCount == 0) return 0;
            continue;
          }


          //print to server
          printf("%s\n", buffer);


           //if there is a command like /vote playername or /role target
          //buffer is now the target or guilty/innocent/abstain
          //HANDLES ROLE STUFF
          if( strncmp(buffer, "/role ", strlen("/role ")) == 0) {
            printf("role sent\n");
            char* temp = parsePlayerCommand(buffer, "/role ");
            printf("temp: '%s'\n", temp);

            if(allPlayers[n].team == T_MAFIA) {
              if(phase == GAMESTATE_NIGHT) {

                int targetID;
                printf("\n\nROLE ACTION!!\n\n");
                if(allPlayers[n].role == R_GODFATHER && mafiaPlayers[R_MAFIOSO].sockd > 0 && mafiaPlayers[R_MAFIOSO].alive) { //godfather and mafioso, godfather sends
                  int mafioso;
                  for(int j = 0; j < MAX_PLAYERS; ++j) {
                    if(allPlayers[j].team == T_MAFIA && allPlayers[j].role == R_MAFIOSO) {
                      mafioso = j;
                      break;
                    }
                  }

                  // for(int i = 0; i < MAX_PLAYERS; ++i) {
                  //   printf("%d\n", allPlayers[i].sockd);
                  // }

                  targetID = roleAction(allPlayers, mafioso, temp);
                  godfatherOverride = 1;
                  if(targetID == -1) singleMessage("Player not found", allPlayers[n].sockd, -1, NULL);
                  else if(allPlayers[targetID].alive == 0) singleMessage("Player is dead", allPlayers[n].sockd, -1, NULL);
                  else if(allPlayers[targetID].team == T_MAFIA) singleMessage("Don't kill your teammates!", allPlayers[n].sockd, -1, NULL);
                  else {
                    sprintf(buffer, "[%d] %s has ordered [%d] %s to ", n, allPlayers[n].name, mafioso, allPlayers[mafioso].name);
                    strcat(buffer, "kill");
                    temp = malloc(BUFFER_SIZE);
                    sprintf(temp, " [%d] %s tonight.", targetID, allPlayers[targetID].name);
                    strcat(buffer, temp);
                    free(temp);
                    sendMessage(buffer, mafiaPlayers, -1);
                    printf("allPlayers[%d].vissiitng: %d\n\n", n, allPlayers[n].visiting);
                  }
                } else if(allPlayers[n].role == R_MAFIOSO && godfatherOverride) { //mafioso, godfather has sent
                  singleMessage("The Godfather has already assigned a target for you.", allPlayers[n].sockd, -1, NULL);
                } else {
                  int targetID = roleAction(allPlayers, n, temp);
                  if(targetID == -1) singleMessage("Player not found", allPlayers[n].sockd, -1, NULL);
                  else if(allPlayers[targetID].alive == 0) singleMessage("Player is dead", allPlayers[n].sockd, -1, NULL);
                  else if(allPlayers[targetID].team == T_MAFIA) singleMessage("Don't kill your teammates!", allPlayers[n].sockd, -1, NULL);
                  else {
                    sprintf(buffer, "[%d] %s has decided to ", n, allPlayers[n].name);
                    if(allPlayers[n].role == R_CONSIGLIERE) strcat(buffer, "investigate");
                    else if(allPlayers[n].role == R_BLACKMAILER) strcat(buffer, "blackmail");
                    else strcat(buffer, "kill");
                    temp = malloc(BUFFER_SIZE);
                    sprintf(temp, " [%d] %s tonight.", targetID, allPlayers[targetID].name);
                    strcat(buffer, temp);
                    free(temp);
                    sendMessage(buffer, mafiaPlayers, -1);
                    printf("allPlayers[%d].vissiitng: %d\n\n", n, allPlayers[n].visiting);
                  }
                }
              }
              else singleMessage("You can only use your role ability during the night.", allPlayers[n].sockd, -1, NULL);
            }
            else if(allPlayers[n].team == T_TOWN || allPlayers[n].team == T_NEUTRAL){
              if(allPlayers[n].role == R_MAYOR){
                if(phase == GAMESTATE_NIGHT || phase == GAMESTATE_RUN_NIGHT){
                  singleMessage("You must wait until day time in order to reveal to the town that you are the mayor.", allPlayers[n].sockd, -1, NULL);
                  continue;
                }
                if(strcmp(temp, allPlayers[n].name) != 0){
                  singleMessage("Are you sure you want to reveal yourself as mayor? If so, your votes will be worth 3x as much. Type /role your-name to reveal yourself.", allPlayers[n].sockd, -1, NULL);
                  continue;
                }
                else{
                  sprintf(buffer, "Attention! Mayor %s has come out of hiding. Their votes are now worth 3x as much.", allPlayers[n].name);
                  sendMessage(buffer, allPlayers, -1);
                  mayorReveal = TRUE;
                }
                continue;
              }

              if(allPlayers[n].role == R_VETERAN){
                char vetAlert[BUFFER_SIZE];
                if(phase == GAMESTATE_NIGHT || phase == GAMESTATE_RUN_NIGHT){
                  if(allPlayers[n].veteranAlert == TRUE){
                    singleMessage("You are alert for enemies that may try to kill you.", allPlayers[n].sockd, -1, NULL);
                  }
                  else{
                    singleMessage("You are not alert. You are sleeping. You can only choose do become alert during the day.", allPlayers[n].sockd, -1, NULL);
                  }
                  continue;
                }
                if(strcmp(temp, allPlayers[n].name) != 0){
                  singleMessage("Are you sure you want to go on alert? If so, type /role your-name to go on alert.", allPlayers[n].sockd, -1, NULL);
                  continue;
                }
                else{
                  if(allPlayers[n].veteranAlert == TRUE){
                    allPlayers[n].veteranAlertCount++;
                    sprintf(vetAlert, "You have decided to NOT go on alert. After tonight you can go on alert only %d more times.", allPlayers[n].veteranAlertCount);
                    singleMessage(vetAlert, allPlayers[n].sockd, -1, NULL);
                    allPlayers[n].veteranAlert = FALSE;
                  }
                  else{
                    if(allPlayers[n].veteranAlertCount > 0){
                      allPlayers[n].veteranAlertCount--;
                      sprintf(vetAlert, "You have decided to go on alert. After tonight you can go on alert only %d more times.", allPlayers[n].veteranAlertCount);
                      singleMessage(vetAlert, allPlayers[n].sockd, -1, NULL);
                      allPlayers[n].veteranAlert = TRUE;
                    }
                    else{
                      singleMessage("You cannot go on alert anymore. You have exhausted yourself.", allPlayers[n].sockd, -1, NULL);
                    }

                  }

                }
                continue;
              }


              if(allPlayers[n].role == R_JAILOR){
                if( ( phase == GAMESTATE_NIGHT || phase == GAMESTATE_RUN_NIGHT ) && allPlayers[n].jailorPrisonerID >= 0){
                  if(strcmp(temp, allPlayers[n].name) != 0){
                    singleMessage("Are you sure you want to execute your prisoner? If so, type /role your-name.", allPlayers[n].sockd, -1, NULL);
                    continue;
                  }
                  else{
                    if(allPlayers[n].executePrisoner == TRUE){
                      singleMessage("You have decided to NOT kill your prisoner.", allPlayers[n].sockd, -1, NULL);
                      singleMessage("The jailor has decided to NOT execute you.", allPlayers[allPlayers[n].jailorPrisonerID].sockd, -1, NULL);
                      allPlayers[n].executePrisoner = FALSE;
                    }
                    else{
                      singleMessage("You have decided to kill your prisoner.", allPlayers[n].sockd, -1, NULL);
                      singleMessage("The jailor has decided to execute you.", allPlayers[allPlayers[n].jailorPrisonerID].sockd, -1, NULL);
                      allPlayers[n].executePrisoner = TRUE;
                    }
                  }
                }
                else{
                  if(strcmp(temp, allPlayers[n].name) == 0){
                    singleMessage("Choose another player to send to jail tonight. Not yourself.", allPlayers[n].sockd, -1, NULL);
                    continue;
                  }
                  else{
                    char jailorMessage[BUFFER_SIZE];
                    //crummy but im tired so whatever
                    int foundPrisoner = FALSE;
                    for(int i = 0; i < MAX_PLAYERS; i++){
                      if( allPlayers[i].sockd > 0 && strcmp(temp, allPlayers[i].name) == 0) {
                        allPlayers[n].jailorPrisonerID = i;
                        printf("jailor prisoner ID: %d",allPlayers[n].jailorPrisonerID);
                        foundPrisoner = TRUE;
                        break;
                      }
                    }
                    if(foundPrisoner){
                      sprintf(jailorMessage, "You have decided to drag %s to prison tonight.", temp);
                    }
                    else{
                      sprintf(jailorMessage, "Player %s not found. Try again.", temp);
                    }
                    singleMessage(jailorMessage, allPlayers[n].sockd, -1, NULL);
                  }
                }
                continue;
            }
            if(allPlayers[n].role == R_RETROBUTIONIST){
                if( ( phase == GAMESTATE_NIGHT || phase == GAMESTATE_RUN_NIGHT ) && allPlayers[n].veteranAlertCount > 0){
                  int target = -1;
                  for(int j = 0; j < MAX_PLAYERS; ++j) {
                    if(deadPlayers[j].sockd > 0 && strcmp(deadPlayers[j].name, temp) == 0) {
                      target = j;
                      break;
                    }
                  }
                  if(target == -1) {
                    singleMessage("That player is either alive, disconnected, or doesn't exist.", allPlayers[n].sockd, -1, NULL);
                  } else {
                    singleMessage("Player successfully selected for revival.", allPlayers[n].sockd, -1, NULL);
                    revivedID = target;
                    allPlayers[n].veteranAlertCount--;
                  }
                }
                else{
                  if(allPlayers[n].veteranAlertCount == 0) singleMessage("You have already revived someone.", allPlayers[n].sockd, -1, NULL);
                  else singleMessage("You may only use your ability during the night.", allPlayers[n].sockd, -1, NULL);
                }
                continue;
            }


              //the below code is for roles that need to VISIT other players. the code above is for roles that stay home

              int targetID = roleAction(allPlayers, n, temp);
              if(targetID == -1) singleMessage("Player not found", allPlayers[n].sockd, -1, NULL);
              else if(allPlayers[targetID].alive == 0) singleMessage("Player is dead", allPlayers[n].sockd, -1, NULL);
              printf("allPlayers[%d].vissiitng: %d\n\n", n, allPlayers[n].visiting);
            }
          }
         else if(strncmp(buffer, "/w ", strlen("/w ")) == 0) {

           if(allPlayers[n].blackmailed == TRUE){
             singleMessage("You cannot whisper. You are blackmailed.", allPlayers[n].sockd, -1, NULL);
             continue;
           }

            printf("whisper sent\n");
            char* temp = parsePlayerCommand(buffer, "/w ");
            int j = -1;
            while(++j < MAX_PLAYERS && alivePlayers[j].sockd > 0 && strncmp(alivePlayers[j].name, temp, strlen(alivePlayers[j].name)) != 0);
            if(j == MAX_PLAYERS) singleMessage("Player not found.", allPlayers[n].sockd, -1, NULL);
            else {
              char spare[BUFFER_SIZE];
              sprintf(spare, "[%d] %s is whispering to [%d] %s.", n, allPlayers[n].name, j, allPlayers[j].name);
              sendMessage(spare, allPlayers, -1);

              singleMessage(temp + strlen(alivePlayers[j].name), allPlayers[j].sockd, n, allPlayers[n].name);

              //blackmailer can read whispers
              if(blackmailerSD > 0){
                char blackmailAppend[BUFFER_SIZE];
                sprintf(blackmailAppend, "       (sent to [%d] %s)", j, alivePlayers[j].name);
                strcat(temp, blackmailAppend);
                singleMessage(temp + strlen(alivePlayers[j].name), blackmailerSD, n, allPlayers[n].name);
              }
            }
          }
          else {

            //HANDLE MESSAGES

            if(allPlayers[n].blackmailed == TRUE){
              if(phase == GAMESTATE_DEFENSE || phase == GAMESTATE_LASTWORDS){
                sendMessage("I am blackmailed.", allPlayers, n);
              }
              else{
                singleMessage("You cannot talk. You are blackmailed.", allPlayers[n].sockd, -1, NULL);
              }
              continue;
            }

            switch(phase){
              case GAMESTATE_DAY:
                sendMessage(buffer, allPlayers, n);
                break;

              case GAMESTATE_DISCUSSION:
                sendMessage(buffer, allPlayers, n);
                break;

              case GAMESTATE_VOTING:
                //find the player struct based on their name that a player voted for
                if(strncmp(buffer, "/vote ", strlen("/vote ")) == 0) {
                  for(int i = 0; i < MAX_PLAYERS; i++){
                  char* temp = parsePlayerCommand(buffer, "/vote ");
                    if( allPlayers[i].sockd > 0 && strcmp(temp, allPlayers[i].name) == 0 ){

                      *votedPlayersList = allPlayers[i];
                      votedPlayersList++;

                      allPlayers[n].votedFor = i;
                      char votingMessage[BUFFER_SIZE] = "";
                      sprintf(votingMessage, "[%d] %s has voted for %s", n, allPlayers[n].name, allPlayers[i].name);
                      sendMessage(votingMessage, allPlayers, -1);
                    }
                  }
                }
                else{
                  sendMessage(buffer, allPlayers, n);
                }
                break;

              case GAMESTATE_DEFENSE:
                if(allPlayers[n].sockd != 0){
                  if(votedPlayer->sockd == allPlayers[n].sockd){
                    sendMessage(buffer, allPlayers, n);
                  }
                  else{
                    singleMessage("Shh... Let the person on trial talk!", allPlayers[n].sockd, -1, NULL);
                  }
                }

                break;

              case GAMESTATE_JUDGEMENT:
              //player on trial cannot vote
              if(votedPlayer->sockd == allPlayers[n].sockd && (strncmp(buffer, "/vote ", strlen("/vote ")) == 0)   ){
                  singleMessage("You cannot vote, you are on trial!", votedPlayer->sockd, -1, NULL);
                  continue;
                }
                if( strncmp(buffer, "/vote ", strlen("/vote ")) == 0) {
                  char* temp = parsePlayerCommand(buffer, "/vote ");
                  if( strcmp(temp, "guilty") == 0 ){
                    strcpy(vote, "guilty");
                    allPlayers[n].whatVote = VOTE_GUILTY;
                  }
                  if( strcmp(temp, "innocent") == 0 ){
                    strcpy(vote, "innocent");
                    allPlayers[n].whatVote = VOTE_INNOCENT;
                  }
                  if( strcmp(temp, "abstain") == 0 ){
                    strcpy(vote, "abstain");
                    allPlayers[n].whatVote = VOTE_ABSTAIN;
                  }
                  if( strlen(vote) > 1 ){
                    char votedMessage[BUFFER_SIZE];
                    sprintf(votedMessage, "[%d] %s ", n, allPlayers[n].name);

                    strcat(votedMessage, "has voted ");
                    strcat(votedMessage, vote);
                    sendMessage(votedMessage, allPlayers, -1);
                  }
                }
                else{
                  sendMessage(buffer, allPlayers, n);
                }

                break;

              case GAMESTATE_LASTWORDS:
                if(allPlayers[n].sockd != 0){
                  if(votedPlayer->sockd == allPlayers[n].sockd){
                    sendMessage(buffer, allPlayers, n);
                  }
                  else{
                    singleMessage("Shh... Let the person on trial talk!", allPlayers[n].sockd, -1, NULL);
                  }
                }

                break;

              case GAMESTATE_NIGHT:
                if(allPlayers[n].sockd > 0 && allPlayers[n].team == T_MAFIA) {
                  colorMessage(buffer, mafiaPlayers, allPlayers[n].role, COLOR_RED);
                }
                if(allPlayers[n].sockd > 0 && allPlayers[n].role == R_MEDIUM) {
                  sendMessage(buffer, deadPlayers, allPlayers[n].role);
                }
                if( allPlayers[n].sockd > 0 && allPlayers[n].role == R_JAILOR && allPlayers[n].jailorPrisonerID >= 0) {
                  singleMessage(buffer, allPlayers[allPlayers[n].jailorPrisonerID].sockd, -2, "Jailor");
                }
                if( n == allPlayers[jailorID].jailorPrisonerID && allPlayers[allPlayers[jailorID].jailorPrisonerID].sockd > 0 ){
                  singleMessage(buffer, allPlayers[jailorID].sockd, n, allPlayers[n].name);
                }
                break;
            }
          }
        }

        // ----- DEAD PLAYERS -----

        if(deadPlayers[n].sockd > 0 && FD_ISSET(deadPlayers[n].sockd, &read_fds)){
          printf("received dead message");
          int bytes = read(allPlayers[n].sockd, buffer, BUFFER_SIZE);
          err(bytes, "bad client read in game loop");
          if(bytes == 0) {
            char name[256];
            strcpy(name, allPlayers[n].name);
            printf("%s disconnected\n", name);
            int sd = allPlayers[n].sockd;
            if(allPlayers[n].team == T_TOWN) {
              --townAlive;
              if(allPlayers[n].role == R_MEDIUM){
                mediumSD = -1;
              }
              if(townAlive == 0) {
                win = T_MAFIA;
                break;
              }
              movePlayer(sd, townPlayers, NULL);
            }
            if(allPlayers[n].team == T_MAFIA) {
              --mafiaAlive;
              if(mafiaAlive == 0) {
                win = T_TOWN;
                break;
              }
              movePlayer(sd, mafiaPlayers, NULL);
            }
            if(allPlayers[n].team == T_NEUTRAL) {
              --neutralAlive;
              movePlayer(sd, neutralPlayers, NULL);
            }
            //printf("removing from all");
            movePlayer(sd, allPlayers, NULL);
            sprintf(buffer, "[%d] %s disconnected", n, name);
            sendMessage(buffer, allPlayers, -1);
            --playerCount;
            if(playerCount == 0) return 0;
            continue;
          } else {
            colorMessage(buffer, deadPlayers, n, 90);
            if(mediumSD > 0){
              singleMessage(buffer, mediumSD, deadPlayers[n].sockd, deadPlayers[n].name);
            }
          }
        }

      }


    }



    nextPhase = 0;
    if(phase == GAMESTATE_DAY) phase = GAMESTATE_NIGHT;
    else phase++;
    if(phase == GAMESTATE_RUN_NIGHT + 1) phase = GAMESTATE_DISCUSSION;
  }

  //PAST HERE SOME GROUP OR TEAM HAS WON OR EVERYONE IS DEAD

  for(int n = 0; n < MAX_PLAYERS; n++){
    if(allPlayers[n].sockd < 0){
      if(win == T_TOWN && allPlayers[n].team == T_TOWN){
        allPlayers[n].hasWon = TRUE;
      }
      if(win == T_MAFIA && allPlayers[n].team == T_MAFIA){
        allPlayers[n].hasWon = TRUE;
      }
    }
  }

  sendMessage("Winners:", allPlayers, -1);

  if(win == T_TOWN){
    if(jesterWin) {
      sprintf(buffer, "Jester: %s", neutralPlayers[R_JESTER].name);
      sendMessage(buffer, allPlayers, -1);
    }
    if(executionerWin) {
      sprintf(buffer, "Executioner: %s", neutralPlayers[R_EXECUTIONER].name);
      sendMessage(buffer, allPlayers, -1);
    }
    strcpy(buffer, "Townies: ");
    char temp[BUFFER_SIZE];
    for(int j = 0; j < MAX_PLAYERS; ++j) {
      if(townPlayers[j].sockd > 0) {
        sprintf(temp, "%s (%s), ", townPlayers[j].name, intToRole(j, T_TOWN));
        strcat(buffer, temp);
      }
    }
    buffer[strlen(buffer)-2] = '\0';
    sendMessage(buffer, allPlayers, -1);
  }
  if(win == T_MAFIA){
    if(jesterWin) {
      sprintf(buffer, "Jester: %s", neutralPlayers[R_JESTER].name);
      sendMessage(buffer, allPlayers, -1);
    }
    if(executionerWin) {
      sprintf(buffer, "Executioner: %s", neutralPlayers[R_EXECUTIONER].name);
      sendMessage(buffer, allPlayers, -1);
    }
    strcpy(buffer, "Mafia: ");
    char temp[BUFFER_SIZE];
    for(int j = 0; j < MAX_PLAYERS; ++j) {
      if(mafiaPlayers[j].sockd > 0) {
        sprintf(temp, "%s (%s), ", mafiaPlayers[j].name, intToRole(j, T_MAFIA));
        strcat(buffer, temp);
      }
    }
    buffer[strlen(buffer)-2] = '\0';
    sendMessage(buffer, allPlayers, -1);
  }


  return 0;
}
