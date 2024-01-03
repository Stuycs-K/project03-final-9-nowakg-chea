# Final Project Proposal

## Group Members:

Gaven Nowak Andrew Che

Gandrew Chowak
       
# Intentions:

Town of C-lem will be a recreation of the game Town of Salem using the C langauge. 
Towm of Salem is a multiplayer game that is similar to the game Mafia where the town discuss amongst themselves to
figure out who the secret mafia members are so they can vote to execute them in the day 
before the mafia kills all of the town members at night.
    
# Intended usage:

A description as to how the project will be used (describe the user interface).

The server computer will run make server.
Each player will run make client (ip adress) to connect to the server.
The entire game will run in the terminal, and the players will be able to type to talk with
each other and put in commands to do actions like /vote (player) or /mafiakill (player).



# Technical Details:

A description of your technical design. This should include:
   
How you will be using the topics covered in class in the project.

Sockets for the server/players connection.
Signals so when people disconnect from the server we can disconnect easily. 
Files so that when the game is done you can read what happened in the entire game by opening the file. 
Allocating memory for most strings. 

How you are breaking down the project and who is responsible for which parts.
  
Andrew: Server/listening to multiple sockets, player chats, starting/ending menu for the game 

Gaven: game interactions, game wins/lose conditions for players, player logic. Signal safe disconnecting, general input parsing including the commands for role specific actions
     
What algorithms and /or data structures you will be using, and how.
    
# Intended pacing:

A timeline with expected completion dates of parts of the project.
