#include "server.h"

#ifndef ROLE_H
#define ROLE_H

#define T_TOWN 0
#define T_MAFIA 1
#define T_NEUTRAL 2

//TOWN ROLES
//above priority 1 (special)
#define R_VETERAN 0 //only when on alert
//priority 1
#define R_MEDIUM 1
#define R_RETROBUTIONIST 2
//priority 3
#define R_DOCTOR 3
//priority 4
#define R_LOOKOUT 4
#define R_SHERIFF 5
//priority 5
#define R_JAILOR 6 //priority 5 when executing
#define R_VIGILANTE 7 //priority 5 when killing someone, top priority when committing suicide
//no night abilities
#define R_MAYOR 8
#define R_MAXTOWNROLE 8

//MAFIA ROLES
//priority 3
#define R_BLACKMAILER 0
//priority 4
#define R_CONSIGLIERE 1
//priority 5
#define R_GODFATHER 2
#define R_MAFIOSO 3
#define R_MAXMAFIAROLE 3

//NEUTRAL ROLES
//priority 5
#define R_SERIALKILLER 0
//no night abilities
#define R_JESTER 1 //haunting is during daytime and thus has no priority number
#define R_EXECUTIONER 2
#define R_MAXNEUTRALROLE 2

#define PUBLIC_EXECUTE 4
#define UNSTOPPABLE_ATTACK 3
#define POWERFUL_ATTACK 2
#define BASIC_ATTACK 1
#define NO_ATTACK 0
//^ attack levels

#define GUARDIAN_ANGEL 4
#define INVINCIBLE_DEFENSE 3
#define POWERFUL_DEFENSE 2
#define BASIC_DEFENSE 1
#define NO_DEFENSE 0
//defense levels

//attack only kills if attack > defense !!!


struct role {
    int attack;
    int defense;
    int priority;
};


//priority: soon to be added... this is a wonderful little thing that will make our lives a lot easier
//all role abilities in the game have a priority from 1 - 6 with 1 being highest 6 being lowered
//implement later




//roles I would think would make a good presentation,  15 people would be needed idk if we could do all this but it would be cool

// town
// 1 jailor (can jail someone for the night which protects them from everyone but the jailor can decide to kill them as well... jailor and prisoner get their own chat)
// 1 sheriff (can investigate someone and guess whether they are sus (every mafia except god father))
// 1 look out (gets to know all the people who visist someone's hosue)
// 1 doctor (heals people)
// 1 medium  (speak with the dead at night)
// 1 mayor (when they decide to reveal themselves in the day as mayor they get 3 votes in public execution trial instead of 1)
// 1 veteran (when they decide to go on "alert" they can shoot everyone who vists them and potential klil mafia / serial killer)
// 1 vigilante (can shoot someone at night, but if they kill a town person they will commit suicide the next day)
// 1 retrobutionist   (revive one person)
//
// mafia
// 1 god father (sends the mafioso to go kill people, if he dies the mafioso becomes god father)
// 1 mafioso  (will be sent to go kill people, takes prio over god father when the mafia decides to kill someone)
// 1 blackmailer (silences a person in the day time, can also read whispers in the game)
// 1 consigliere (reveals roles of whoever they visit)
//
//
// neutral
// 1 jester (wants to be public executed) or 1 executioner (wants to execute someone)
// 1 serial killer (kills one person every day)

char* intToTeam(int i);
char* intToRole(int role, int team);
int roleAction(struct player* allPlayers, int senderID, char* target);
#endif
