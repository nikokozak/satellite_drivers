#include "command_parser.h"

// No variable definitions here - using the extern references to the variables defined in plotter.ino

// Reset the command state
void resetCommand(struct Command *cmd) {
  cmd->cmd = '\0';
  for (int i = 0; i < MAX_ARGS; i++) {
    cmd->args[i] = 0;
  }
  cmd->numArgs = 0;
  cmd->valid = false;
  bufferPos = 0;
  parseState = WAITING_FOR_COMMAND;
  currentArg = 0;
}

// Check if the command character is valid
bool isValidCommandChar(char cmd) {
  switch (cmd) {
    case 'x': // X motor command
    case 'y': // Y motor command
    case 'c': // Calibration command
    case 'a': // Auto Calibration command
    case 'h': // Home command
    case 's': // Status command
    case 'p': // Set origin command
    case 'g': // Go To position command
    case 'm': // Mark min during calibration
    case 'M': // Mark max during calibration
    case 'q': // Quit calibration
      return true;
    default:
      return false;
  }
}

// Function to convert string to integer for Arduino
int stringToInt(const char* str) {
  int result = 0;
  bool negative = false;
  int i = 0;
  
  // Skip whitespace
  while (str[i] == ' ' || str[i] == '\t') {
    i++;
  }
  
  // Handle sign
  if (str[i] == '-') {
    negative = true;
    i++;
  } else if (str[i] == '+') {
    i++;
  }
  
  // Convert digits
  while (str[i] >= '0' && str[i] <= '9') {
    result = result * 10 + (str[i] - '0');
    i++;
  }
  
  return negative ? -result : result;
}

// Function to check if a character is a digit
bool isDigit(char c) {
  return (c >= '0' && c <= '9');
}

// Parse a complete argument from the buffer
int parseArgument() {
  cmdBuffer[bufferPos] = '\0';
  bufferPos = 0;
  
  // Check if the buffer is empty or just spaces
  for (int i = 0; cmdBuffer[i] != '\0'; i++) {
    if (cmdBuffer[i] != ' ' && cmdBuffer[i] != '\t') {
      // Found a non-space character, try to parse
      return stringToInt(cmdBuffer);
    }
  }
  
  // Buffer was empty or just spaces
  return 0;
}

// Process incoming character for command parsing
void processCommandChar(char incomingChar, struct Command *cmd) {
  // Handle end of line (complete the command)
  if (incomingChar == '\n' || incomingChar == '\r') {
    if (parseState == READING_ARGUMENT && bufferPos > 0) {
      cmd->args[currentArg] = parseArgument();
      cmd->numArgs = currentArg + 1;
      cmd->valid = true;
    } else if (cmd->cmd != '\0' && parseState != WAITING_FOR_COMMAND) {
      // Complete command without argument or after comma
      cmd->valid = true;
    }
    return;
  }
  
  // Handle different parser states
  switch (parseState) {
    case WAITING_FOR_COMMAND:
      // Skip spaces before command
      if (incomingChar == ' ' || incomingChar == '\t') {
        return;
      }
      
      // First character must be a valid command
      if (isValidCommandChar(incomingChar)) {
        cmd->cmd = incomingChar;
        parseState = READING_ARGUMENT;
        bufferPos = 0;
      } else {
        // Invalid command character, reset and try again
        Serial.print(F("Invalid command character: "));
        Serial.println(incomingChar);
        resetCommand(cmd);
      }
      break;
      
    case READING_ARGUMENT:
      // Handle comma (separates arguments)
      if (incomingChar == ',') {
        // Complete current argument and prepare for next
        cmd->args[currentArg] = parseArgument();
        currentArg++;
        
        if (currentArg >= MAX_ARGS) {
          // Too many arguments
          Serial.println(F("Error: Too many arguments"));
          resetCommand(cmd);
        } else {
          parseState = WAITING_FOR_NEXT_ARG;
        }
        return;
      }
      
      // Add digit or sign to buffer
      if ((isDigit(incomingChar) || incomingChar == '-' || incomingChar == '+' || 
           incomingChar == ' ' || incomingChar == '\t') && 
          bufferPos < BUFFER_SIZE - 1) {
        cmdBuffer[bufferPos++] = incomingChar;
      }
      break;
      
    case WAITING_FOR_NEXT_ARG:
      // Skip spaces after comma
      if (incomingChar == ' ' || incomingChar == '\t') {
        return;
      }
      
      // Start reading next argument
      if (isDigit(incomingChar) || incomingChar == '-' || incomingChar == '+') {
        parseState = READING_ARGUMENT;
        bufferPos = 0;
        cmdBuffer[bufferPos++] = incomingChar;
      } else {
        // Unexpected character after comma
        Serial.print(F("Error: Expected number after comma, got: "));
        Serial.println(incomingChar);
        resetCommand(cmd);
      }
      break;
  }
}

// Read serial input and parse command
void readSerialCommand() {
  if (!Serial.available()) return;
  
  char incomingChar = Serial.read();
  
  // Process the character within our state machine
  processCommandChar(incomingChar, &command);
} 