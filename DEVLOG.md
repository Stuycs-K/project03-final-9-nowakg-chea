# Dev Log:

## Gaven

### 2024-01-02 - begining work
worked on Proposal
researched some town of salem roles
added dev log format

### 2024-01-03 - Brief description
worked on makefile

### 2024-01-04 - Brief description
worked on player structs work on role struct beginning to work on server and client select

### 2024-01-05 THROUGH 2024-01-07 - Brief description
worked on having the server send messages to players (chat feature)

trying to make a FD_ISSET to a pipe read file descriptor so that a subserver can use a timer
and tell the parent server when day/night/voting time cycles are over  

made client read and write and server read and write at the same time during the game
but for some reason the select in server blocks server i have to investigate
the FD_ISSET to a pipe read compiles and the forking server doesnt crash the game so thats good
but i can't test if it works until server goes past the select stage

### 2024-01-08 - Brief description
Made the chat UI work so that players that send messages wouldn't get their same msg back from the server
and all sent messages go to all players
worked on voting system and game phases and command parsing when players do /role or /vote


### 2024-01-09 - Brief description
worked on voting system
got rid of player.h because it was redundant with server.h
STILL NEED TO TEST IT!
STILL WORKING ON VOTING

### 2024-01-10 - Brief description
STILL WORKING ON VOTING and we fixed some player organization in alive players, dead players, disconnected/connceted players

### 2024-01-11 - Brief description
VOTING DONE!
starting on role specific stuff

current bugs: client joining is a queue and not spontaneous
                voting works but when you try to vote a player and they dont get killed the timer moves on to the next phase

fixing voting timer bug
### 2024-01-12 THROUGH 2024-01-14 - Brief description
making priority and role system

### 2024-01-15 - Brief description
worked on mayor (and revealing)
fixed voting bug that allowed people to vote a bunch of times for the same person
finished veteran, vigilante
worked on medium
implemented blackmailer
implemented jailor
implemented serial killer

READ ME !!!!


## Andrew

### 2024-01-02 - Preparation
Researched Town of Salem roles, worked on readme

### 2024-01-03 - File organizing, terminal I/O functions
Reorganized file structure and deleted networking.c/.h, added input.c for special printing functions

### 2024-01-04 - Join phase functionality
Added color printing to input.c and wrote new server and client logic in their mains to handle the join phase logic. I think join phase is good for now, only problem is a you have to start a client and send !start as your username to start the game

### 2024-01-05 THROUGH 2024-01-07 - Role assignments, timer subserver with basic game phases
Wrote a random role assigner. Because it goes in join order and there's a lot more town slots, later joining players are much more likely to be town, so not truly random, but good enough.
Also added constants for roles and functions to convert those constants to the names for convenience.
Added game phase constants and a full separate timer subserver process that communicates with the server. Fixed the select looping that Gaven started.

### 2024-01-08 - Chat beautification, basic client disconnect logic
I made clients locally color their own messages as blue and all server messages are sent colored in green.
I added checks for clients disconnecting with a message and chat and made the server stop if all players disconnect.

### 2024-01-09 - Brief description
I expanded the player disconnect checks and had to rewrite a lot to fix a bug where disconnecting would lock all players with higher IDs out of the game.

### 2024-01-10 - Brief description
I worked on making mafia able to kill people, and implemented the alive/deadPlayers arrays and made a dead chat to show killing effects.
There's still no checks, e.g. mafia can't win and you can kill yourself/your teammates.

### 2024-01-11 - Brief description
I made the server announce player deaths in the morning and mafia killing decisions in mafia chat and fixed some bugs with the private chats.
I also merged our branches and fixed Gaven's phase repeating bug.

### 2024-01-12 THROUGH 2024-01-14 - Brief description
Fixed a bug where killing player with id 0 would say player not found.
Added player-to-player whispering with gray font.

### 2024-01-15 - Brief description
Implemented retrobutionist godfather-mafioso interaction, fixed killing/executing players, made chats different colors, and fixed a lot of minor bugs with Gaven
