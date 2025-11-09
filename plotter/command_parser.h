#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>
#include "types.h"

// Function Prototypes for command parsing
void resetCommand(struct Command *cmd);
bool isValidCommandChar(char cmd);
int stringToInt(const char* str);
bool isDigit(char c);
int parseArgument();
void processCommandChar(char incomingChar, struct Command *cmd);
void readSerialCommand();

// Declare external variables needed by parser functions
// These will be defined in plotter.ino
extern char cmdBuffer[];
extern int bufferPos;
extern ParseState parseState;
extern int currentArg;
extern struct Command command; // Global command instance

#endif // COMMAND_PARSER_H 