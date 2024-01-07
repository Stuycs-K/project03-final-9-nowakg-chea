#ifdef PLAYER_H
#define PLAYER_H

#define MAX_PLAYERS 15
#define PLAYER_ALIVE 1
#define PLAYER_DEAD 0



struct playerMessage{
    struct player player;
    char[BUFFER_SIZE] message;
};

struct player{
    char[BUFFER_SIZE] name;
    struct role role;
    int dead;
    //file descriptor for select stuff
};



void sendMessage(struct playerMessage data);



//chats will just made as a list of the stuct players
//Mafia team will just be a list of struct players, and then when you send a message you can send a message

#endif
