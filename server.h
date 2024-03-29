#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>


#ifndef SERVER_H
#define SERVER_H

#define PORT "19230"
#define BUFFER_SIZE 1024

#define MAX_PLAYERS 15
#define PLAYER_ALIVE 1
#define PLAYER_DEAD 0

#define VOTE_ABSTAIN 0
#define VOTE_GUILTY 1
#define VOTE_INNOCENT 2

#define TRUE 1
#define FALSE 0

#define GAMESTATE_DAY 0
#define GAMESTATE_DISCUSSION 1
#define GAMESTATE_VOTING 2
#define GAMESTATE_DEFENSE 3
#define GAMESTATE_JUDGEMENT 4
#define GAMESTATE_VOTE_COUNTING 5
#define GAMESTATE_LASTWORDS 6
#define GAMESTATE_KILL_VOTED 7
#define GAMESTATE_NIGHT 8
#define GAMESTATE_RUN_NIGHT 9

#define PIPE_READ 0
#define PIPE_WRITE 1


struct player {
  int sockd; char name[16]; int alive; int votesForTrial; int whatVote; int votedFor; int team; int role; int hasWon;
  int attack; int defense; int rolePriority; int addedAttack; int addedDefense;
  int visitorsID[MAX_PLAYERS]; int visiting;
  int veteranAlert; int veteranAlertCount;
  int vigilanteBullets;
  int jailorPrisonerID; int executePrisoner;
  int blackmailed;
};
void err(int i, char*message);
void movePlayer(int sd, struct player* from, struct player* to);
//void serverStart();

#endif
