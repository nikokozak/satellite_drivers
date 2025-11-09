#ifndef TYPES_H
#define TYPES_H

#include "config.h" // Include config for MAX_ARGS

// Command Struct - Enhanced version
#define MAX_ARGS 4 // Keeping this here as it's tied to the struct
struct Command {
  char cmd;
  int args[MAX_ARGS];
  int numArgs;
  bool valid;
};

// State machine states for command parsing
enum ParseState {
  WAITING_FOR_COMMAND,
  READING_ARGUMENT,
  WAITING_FOR_NEXT_ARG
};

#endif // TYPES_H 