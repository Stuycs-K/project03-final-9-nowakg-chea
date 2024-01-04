#include 'server.h'

#ifdef PLAYER_H
#define PLAYER_H

#define MAX_PLAYERS 10
#define ALIVE 1;
#define DEAD 0;

struct playerMessage{
    struct player player;
    char[BUFFER_SIZE] message;
}

//should we put destination in sendMessage or in the struct? 

void sendMessage(struct playerMessage data, struct playerGroup[MAX_PLAYERS]);

struct player {
    char[BUFFER_SIZE] name;
    struct role role;
    int dead;
    //file descriptor for select stuff
}


//chats will just made as a list of the stuct players
//Mafia team will just be a list of struct players, and then when you send a message you can send a message 