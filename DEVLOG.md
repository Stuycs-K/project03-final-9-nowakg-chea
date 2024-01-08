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


### 2024-01-09 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-10 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-11 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-12 THROUGH 2024-01-14 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-15 - Brief description
Expanded description including how much time was spent on task.


## Andrew

### 2024-01-02 - Brief description
Researched Town of Salem roles, worked on readme

### 2024-01-03 - Brief description
Reorganized file structure and deleted networking.c/.h, added input.c for special printing functions

### 2024-01-04 - Brief description
Added color printing to input.c and wrote new server and client logic in their mains to handle the join phase logic. I think join phase is good for now, only problem is a you have to start a client and send !start as your username to start the game

### 2024-01-05 THROUGH 2024-01-07 - Brief description
Wrote a random role assigner. Because it goes in join order and there's a lot more town slots, later joining players are much more likely to be town, so not truly random, but good enough.
Also added constants for roles and functions to convert those constants to the names for convenience.
Added game phase constants and a full separate timer subserver process that communicates with the server. Fixed the select looping that Gaven started.

### 2024-01-08 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-09 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-10 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-11 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-12 THROUGH 2024-01-14 - Brief description
Expanded description including how much time was spent on task.

### 2024-01-15 - Brief description
Expanded description including how much time was spent on task.
