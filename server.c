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
      case GAMESTATE_DAY: time = 3; break; //originally 15
      case GAMESTATE_DISCUSSION: time = 3; break; //originally 45
      case GAMESTATE_VOTING: time = 30; break;
      case GAMESTATE_DEFENSE: time = 20; break;
      case GAMESTATE_JUDGEMENT: time = 20; break;
      case GAMESTATE_VOTE_COUNTING: time = 1; break;
      case GAMESTATE_LASTWORDS: time = 7; break;
      case GAMESTATE_KILL_VOTED: time = 5; break;
      case GAMESTATE_NIGHT: time = 37; break;
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
      allPlayers[i].team = T_NEUTRAL;
      allPlayers[i].role = R_EXECUTIONER;
      alivePlayers[i].team = T_NEUTRAL;
      alivePlayers[i].role = R_EXECUTIONER;
      neutralPlayers[R_EXECUTIONER] = allPlayers[i];
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
  //used for win conditions LATER

  // used for later when dead people need to chat to the medium
  int mediumSD = -1;

  //for when jailor needs to jail his prisoner
  int jailorSD = -1;

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
    for(int n = 0; n < MAX_PLAYERS; n++){
      allPlayers[i].visitorsID[n] = -1;
    }


    if(allPlayers[i].team == T_TOWN){
      townAlive++;
      switch (allPlayers[i].role){
        case R_VETERAN:
          allPlayers[i].attack = POWERFUL_ATTACK;
          allPlayers[i].rolePriority = 1;
          break;
        case R_MEDIUM:
          mediumSD = allPlayers[i].sockd; //for later that medium can talk to dead at night
          break;
        case R_RETROBUTIONIST:
          allPlayers[i].rolePriority = 1;
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
          jailorSD = allPlayers[i].sockd; //for later when jailor jails people
          allPlayers[i].attack = UNSTOPPABLE_ATTACK;
          allPlayers[i].rolePriority = 5;
          break;
        case R_VIGILANTE:
          allPlayers[i].attack = BASIC_ATTACK;
          allPlayers[i].rolePriority = 5;
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
          break;
        }



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

  //BEGIN THE GAME

  int nextPhase = 0;
  int phase = GAMESTATE_DAY;
  struct player* votedPlayersList = votedPlayers;

  int votingTries = 3;
  struct player* votedPlayer = NULL;
  int guiltyVotes = 0, innoVotes = 0, abstVotes = 0; //will be used in the GAMESTATE_JUDGEMENT phase


  int win = -1; //win will be equal to the team that wins so like T_MAFIA or T_TOWN

  printf("\n\nBEGINNING GAME!\n\n");
  while(win < 0){
    printf("new phase: %d\n", phase);
    if(phase == GAMESTATE_DISCUSSION) {

        //move dying players to dead players and reset everyone's addedAttack and addedDefense
      for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(dyingPlayers[i].sockd > 0) {
          allPlayers[i].alive = FALSE;
          deadPlayers[i] = dyingPlayers[i];
          if(deadPlayers[i].team == T_TOWN && deadPlayers[i].alive == FALSE){
            townAlive--;
            if(allPlayers[i].role == R_MEDIUM){
              mediumSD = -1;
            }
          }
          if(deadPlayers[i].team == T_MAFIA && deadPlayers[i].alive == FALSE){
            mafiaAlive--;
          }
          if(deadPlayers[i].team == T_NEUTRAL && deadPlayers[i].alive == FALSE){
            neutralAlive--;
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
    if(mafiaAlive == 0){
      win = T_TOWN;
      break;
    }
    if(townAlive == 0){
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
                  }
                }
                sprintf(buffer,"It is time to vote! You have %d tries left to vote to kill a player. Use /vote player_name to vote to put a player on trial.",votingTries);
                sendMessage(buffer, allPlayers, -1);
                break;
              case GAMESTATE_DEFENSE:
                 //WILL NEED TO CHANGE TO ALIVE PLAYRES LATER BUT THIS IS FINE FORE NOW
                sendMessage("Counting votes...", allPlayers, -1);
                int highestVote = 0;
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
                    }
                    if(allPlayers[n].whatVote == VOTE_INNOCENT){
                      innoVotes++;
                    }
                    if(allPlayers[n].whatVote == VOTE_GUILTY){
                      guiltyVotes++;
                    }
                  }
                }
                if(guiltyVotes <= innoVotes){
                  sprintf(buffer, " has not enough votes to be killed. You now have %d tries left to vote a player.", votingTries );
                  sendMessage(buffer, allPlayers, -1);
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
                  }

                  if(votedPlayer->sockd == allPlayers[executionerTarget].sockd) {
                    singleMessage("Congratulations! You won. Stick around and you will be able to side with either town or mafia but you win either way.", neutralPlayers[R_EXECUTIONER].sockd, -1, NULL);
                  }

                  //KILL THE GUILTY PLAYER!!!
                  movePlayer(votedPlayer->sockd, alivePlayers, deadPlayers);
                  votedPlayer->alive = FALSE;
                  singleMessage("You have been killed! You can now talk with other dead players.", votedPlayer->sockd, -1, NULL);
                  --playerCount;

                  votedPlayer = NULL;
                  phase = GAMESTATE_NIGHT;
                  continue;
                }
                break;

              case GAMESTATE_NIGHT:
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
                          if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                            movePlayer(allPlayers[n].sockd, dyingPlayers, NULL);
                            sprintf(visitorRelay, "You killed %s!", allPlayers[n].name);
                          }
                          else{
                            sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, theikr defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                          }
                          break;
                        }
                    }
                    else if(allPlayers[allPlayers[n].visitorsID[i]].team == T_MAFIA){
                        switch ( allPlayers[allPlayers[n].visitorsID[i]].role ){

                          case R_GODFATHER:
                            if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                              movePlayer(allPlayers[n].sockd, dyingPlayers, NULL);
                              sprintf(visitorRelay, "You killed %s!", allPlayers[n].name);
                            }
                            else{
                              sprintf(visitorRelay, "You tried to kill %s but their defence was too high! your attack %d, theikr defense: %d, their added defense: %d", allPlayers[n].name, allPlayers[allPlayers[n].visitorsID[i]].attack , allPlayers[n].defense, allPlayers[n].addedDefense  );
                            }
                            break;
                          case R_MAFIOSO:
                            if(allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].defense && allPlayers[allPlayers[n].visitorsID[i]].attack > allPlayers[n].addedDefense){
                              movePlayer(allPlayers[n].sockd, dyingPlayers, NULL);
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
                            sprintf(visitorRelay, "You have blackmailed %s, and they cannot talk for the next day!", allPlayers[n].name);
                            break;
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
        if(time == 120 || time == 60 || time == 30 || time == 15 || time == 10 || time == 5 || time == 4 || time == 3 || time == 2|| time == 1) printf("%ds left\n", time);
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
            if(allPlayers[n].team == T_TOWN) movePlayer(sd, townPlayers, NULL);
            if(allPlayers[n].team == T_MAFIA) movePlayer(sd, mafiaPlayers, NULL);
            if(allPlayers[n].team == T_NEUTRAL) movePlayer(sd, neutralPlayers, NULL);
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
            printf("temp: %s\n", temp);

            if(allPlayers[n].team == T_MAFIA) {
              if(phase == GAMESTATE_NIGHT) {
                printf("\n\nROLE ACTION!!\n\n");
                int targetID = roleAction(allPlayers, n, temp);
                if(targetID == -1) singleMessage("Player not found", allPlayers[n].sockd, -1, NULL);
                else if(allPlayers[targetID].alive == 0) singleMessage("Player is dead", allPlayers[n].sockd, -1, NULL);
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
              else singleMessage("You can only use your role ability during the night.", allPlayers[n].sockd, -1, NULL);
            }
            else if(allPlayers[n].team == T_TOWN){
              int targetID = roleAction(allPlayers, n, temp);
              if(targetID == -1) singleMessage("Player not found", allPlayers[n].sockd, -1, NULL);
              else if(allPlayers[targetID].alive == 0) singleMessage("Player is dead", allPlayers[n].sockd, -1, NULL);
              printf("allPlayers[%d].vissiitng: %d\n\n", n, allPlayers[n].visiting);
            }
          }
         else if(strncmp(buffer, "/w ", strlen("/w ")) == 0) {
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
            }
          }
          else {

            //HANDLE MESSAGES

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
                      allPlayers[i].votesForTrial++;
                      sprintf(buffer, "[%d] %s has voted", n, allPlayers[n].name);
                      sendMessage(buffer, allPlayers, n);
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
                    printf("sock %d\n", allPlayers[n].sockd);
                    sendMessage(buffer, allPlayers, n);
                  }
                  else{
                    singleMessage("Shh... Let the person on trial talk!", allPlayers[n].sockd, -1, NULL);
                  }
                }

                break;

              case GAMESTATE_JUDGEMENT:
              //player on trial cannot vote
              if(votedPlayer->sockd == allPlayers[n].sockd){
                  singleMessage("You cannot vote, you are on trial!", votedPlayer->sockd, -1, NULL);
                  continue;
                }
                if( strncmp(buffer, "/vote ", strlen("/vote ")) == 0) {
                  char* temp = parsePlayerCommand(buffer, "/vote ");
                  if( strcmp(temp, "guilty") == 0 ){
                  guiltyVotes++;
                  strcpy(vote, "guilty");
                  }
                  if( strcmp(temp, "innocent") == 0 ){
                    innoVotes++;
                    strcpy(vote, "innocent");
                  }
                  if( strcmp(temp, "abstain") == 0 ){
                    abstVotes++;
                    strcpy(vote, "abstain");
                  }
                  if( strlen(vote) > 1 ){
                    sprintf(buffer, "[%d] %s ", n, allPlayers[n].name);
                    strcat(buffer, "has voted ");
                    strcat(buffer, vote);
                    sendMessage(buffer, allPlayers, n);
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
                  sendMessage(buffer, mafiaPlayers, allPlayers[n].role);
                }
                if(allPlayers[n].sockd > 0 && allPlayers[n].role == R_MEDIUM) {
                  sendMessage(buffer, deadPlayers, allPlayers[n].role);
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
            if(allPlayers[n].team == T_TOWN) movePlayer(sd, townPlayers, NULL);
            if(allPlayers[n].team == T_MAFIA) movePlayer(sd, mafiaPlayers, NULL);
            if(allPlayers[n].team == T_NEUTRAL) movePlayer(sd, neutralPlayers, NULL);
            //printf("removing from all");
            movePlayer(sd, allPlayers, NULL);
            sprintf(buffer, "[%d] %s disconnected", n, name);
            sendMessage(buffer, allPlayers, -1);
            --playerCount;
            if(playerCount == 0) return 0;
            continue;
          } else {
            sendMessage(buffer, deadPlayers, n);
            if(mediumSD > 0){
              singleMessage(buffer, mediumSD, deadPlayers[n].sockd, deadPlayers[n].name);
            }
          }
        }

      }


    }



    nextPhase = 0;
    phase++;
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

  if(win == T_TOWN){
    sendMessage("THE TOWN HAS WON!", allPlayers, -1);
  }
  if(win == T_MAFIA){
    sendMessage("THE MAFIA HAS WON!", allPlayers, -1);
  }


  return 0;
}
