#include <stdio.h>
#include <string.h>

#ifndef INPUT_H
#define INPUT_H
void clearScreen();
void query(char* message, char* response, int buffSize);
void moveCursor(int x, int y);
#endif
