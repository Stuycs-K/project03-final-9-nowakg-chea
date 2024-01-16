[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/SQs7pKlr)
# Town of C-lem

### Gandrew Chowak

Andrew Che, Gaven Nowak

### Project Description:

We are recreating a basic version of the video game "Town of Salem." In the game, there is a good team (Town) and an evil team (Mafia). The Mafia try to kill the entire Town every night, while the Town tries to figure out who is in the Mafia and vote to execute them in the morning. Within each team there are special roles, such as the Town's Mayor who has more voting power and the Mafia's Godfather who has his Mafioso do the killing for him to avoid getting caught.

### Instructions:

How does the user install/compile/run the program.
  - install the C files, H files and the makefile inside of a file  
  - then, go to your terminal on the computer you want to host the game on and type 'make server' to setup the game server
  - then, go to your terminal on the computer you want to play the game on and type 'make client (IP ADDRESS OF SERVER)' to connect to the game server

How does the user interact with this program?
  - on the game server, once enough people have joined the game you can type /start to start the game
  - on the player side, enter your name once you connect and wait for the server to start the game

  GENERAL GAMEPLAY

  You play the role of... somebody... in a town called C-lem. (Because everyone speaks "C").
  If you are a town member, you must kill all of the members of the mafia and you must kill the serial killer.
  If you are a mafia member, you must kill all of the members of the town and the serial killer.
  If you are the serial killer, you have to kill... everyone.

  Every day, you can talk in the chat. Just simply type in the terminal to talk.
  You can use /w player-name to whisper to someone. However, be careful: The town will know that you are whispering to that person (but not what you are saying).

  Then, after some time, the town will be able to vote to put a player on trial.
  To vote, type /vote player-name .
  Then, after enough votes have been cast for a person, that person will be put on the stand. Nobody will be able to talk besides them and they will give reasoning
  as to why they are not guilty.
  Then, the town will have a chance to vote on whether they believe that person is innocent or guilty (or they can abstain, too if they wish not to vote).
  If there are more guilty votes than innocent votes the person will be publicly executed. Then, their role will be revealed.
  If there are more innocent votes than guilty votes then the person will be exonerated and then the town will have a chance to vote to put another person on trial.
  The town can do this up to three times per day cycle.

  Once the day cycle is over by either getting a public execution or by putting someone on trial three times, night comes.

  During the night, most roles get to do their action. Certain roles can be activated in the day, and some roles can see what other roles do.
  Depending on what role you have, you may be able to save, kill, or seek out information from other players.

  Whatever happens, in the morning, whoever dies will be announced to be dead and their role will be revealed.

  Here is a rundown of all of the roles implemented in the game and what they do and how they win:
  All players use /role (target) to do their action. If your action is something that has no target you do /role (your name).


  TOWN: (Wins when all Mafia are dead and serial killer is dead)

  Veteran: During the day, you can choose to go on alert during the next night. When alert, anyone who visits you will be killed if they have a defense of less than Powerful.
  Medium: During the night, you can talk to the dead.
  Retributionist: During the night, you can raise someone from the dead.
  Doctor: During the night, you can heal someone, giving them a Powerful defense.
  Lookout: During the night, you can observe someone's home and see who visits them.
  Sheriff: During the night, you can observe someone and determine if they are a criminal or not.
  Jailor: During the day, you can choose to send someone to jail. Then, at night, you can choose whether or not you want to execute them in jail. You are anonymous to your prisoner and you can talk to each other. Being in jail gives your prisoner Powerful defense.
  Vigilante: During the night, you can choose to shoot someone. This is an attack with the level of Basic and if you shoot a towns member, you will die from guilt on the next day.
  Mayor: During the day, you can reveal yourself to be the Mayor, which then makes your votes worth triple in sending people to trial and determining if they are guilty or not.


  MAFIA: (wins when all Town are dead and serial killer is dead)

  During the night all the mafia members are able to speak to each other.
  Godfather: During the night you and the mafioso will choose who to kill. Your decision overrides the mafioso's and the mafioso will always be the one to go and murder the player that is selected. You will not appear
  as a criminal to the Sheriff.
  Mafioso: During the night you and the Godfather will choose who to kill. If the Godfather dies you become the Godfather in spirit... not in the game though.
  Consigliere: During the night you can go out and figure out a player's role.
  Blackmailer: During the night you can go out and blackmail someone, which prevents them from talking the next day. Also, you get to hear everyone's whispers.


  NEUTRAL: (They have their own win conditions)

  Serial Killer: During the night, go out and kill someone. You must kill everyone to win. You will not appear as a criminal to the Sheriff.
  Executioner: The game will randomly select you a target. You must publicly execute this person in order to win. What happens after that doesn't really matter to you, so you can side with Mafia or Town.
  Jester: You must convince the town to publicly execute you. Then, you get to haunt someone and kill them. What happens after that doesn't really matter to you, so you can side with Mafia or Town.











   BUGS:

   
