#include <stdio.h>
#include <string.h>

#ifndef INPUT_H
#define INPUT_H

#define COLOR_RED 31
#define COLOR_GRAY 37
#define COLOR_RESET 0
void clearScreen();
void query(char* message, char* response, int buffSize);
void moveCursor(int x, int y);
void colorText(int color);
#endif
