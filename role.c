#include "role.h"

char* intToTeam(int i) {
    if(i == T_TOWN) return "Town";
    if(i == T_MAFIA) return "Mafia";
    if(i == T_NEUTRAL) return "Neutral";
    return "Error: invalid team num";
}

char* intToRole(int role, int team) {
    if(team == T_TOWN) {
        if(role == R_VETERAN) return "Veteran";
        if(role == R_MEDIUM) return "Medium";
        if(role == R_RETROBUTIONIST) return "Retrobutionist";
        if(role == R_DOCTOR) return "Doctor";
        if(role == R_LOOKOUT) return "Lookout";
        if(role == R_SHERIFF) return "Sheriff";
        if(role == R_JAILOR) return "Jailor";
        if(role == R_VIGILANTE) return "Vigilante";
        if(role == R_MAYOR) return "Mayor";
    }
    if(team == T_MAFIA) {
        if(role == R_BLACKMAILER) return "Blackmailer";
        if(role == R_CONSIGLIERE) return "Consigliere";
        if(role == R_GODFATHER) return "Godfather";
        if(role == R_MAFIOSO) return "Mafioso";
    }
    if(team == T_NEUTRAL) {
        if(role == R_SERIALKILLER) return "Serial Killer";
        if(role == R_JESTER) return "Jester";
        if(role == R_EXECUTIONER) return "Executioner";
    }
    return "Error: invalid team or role num";
}

//if the destination player list is NULL, it removes the player from the game as if they never joined
//if shift is true, then the players
void movePlayer(int sd, struct player* from, struct player* to) {
  int i = -1;
  while(from[++i].sockd != sd)
    if(i >= MAX_PLAYERS) {
      printf("Missing sd in removePlayer\n");
      return;
    }
  //i is now the location of the target player
  /*
  if(to != NULL) {
    int j = -1;
    while(to[++j].sockd <= 0); //moves j to next open slot in array
    to[j] = from[i];
  }
  */
  to[i] = from[i];
  from[i].sockd = 0;
}

//if action successful, returns id of target
//if cant find player, returns -1
//if sender is roleblocked, returns -2
//if target defense is too high, returns -3
int roleAction(struct player* allPlayers, int senderID, char* target) {
    int targetID = -1;
    for(int i = 0; i < MAX_PLAYERS; ++i) {
        if(allPlayers[i].sockd > 0) {
            printf("roleAction %s %s\n", allPlayers[i].name, target);
            if(strcmp(allPlayers[i].name, target) == 0) {
                targetID = i;

                allPlayers[senderID].visiting = targetID;
                printf("allPlayers[%d] setting its visiting to %d\n", senderID, targetID);
                printf("allapleyurs :%d %d\n", senderID, allPlayers[senderID].visiting);
                printf("all: %s\n", allPlayers[senderID].name);

                break;
            }
        }
    }

    return targetID;
}
