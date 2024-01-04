#include "input.h"

//clears terminal, moves cursor to top left
void clearScreen() {
    printf("\033[2J\033[H");
}

//prints message then stores user input in response (automatically clears newlines)
void query(char* message, char* response, int buffSize) {
    printf("%s\n", message);
    fgets(response, buffSize, stdin);
    if(response[strlen(response)-1] == '\n') response[strlen(response)-1] = '\0';
    if(response[strlen(response)-1] == '\r') response[strlen(response)-1] = '\0';
}

void moveCursor(int x, int y) {
    printf("\033[%d;%dH", y, x);
}

int main() {
    clearScreen();
    char s[256];
    query("testing", s, 256);
    moveCursor(0, 10);
    printf("'%s'\n", s);
    return 0;
}