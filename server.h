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

struct player {int sockd; char name[16]; int alive; int team; int role;};

void err(int i, char*message);
//void serverStart();

#endif
