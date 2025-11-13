#include <Arduino.h>
#include "config.h" // Include the new configuration file
#include "types.h" // Include the new types header
#include "command_parser.h" // Include command parsing functions

#include <stdarg.h>

// Safety checking flag (remains here as it's a variable)
bool isCalibrating = false;  // Flag to indicate we're in calibration mode
bool shouldInterrupt = false; // Simple flag for interrupting movement

// Plotter limits and position tracking (variables remain here)
int xMin = 0;
int xMax = 0;
int yMin = 0;
int yMax = 0;
int xCurrent = 0;
int yCurrent = 0;

// Command Buffer
char cmdBuffer[BUFFER_SIZE];
int bufferPos = 0;

// Global command instance and state (remain here)
struct Command command;
ParseState parseState = WAITING_FOR_COMMAND;
int currentArg = 0;

// Function Prototypes
void showCurrentPosition();
void processCommand(struct Command cmd);
void calibrateXAxis();
void calibrateYAxis();
void homeToOrigin();
void setCurrentPositionAsOrigin();
void goToPosition(int targetX, int targetY);
void autoCalibrate();
void findAxisLimit(int motor, int direction, int limitSwitch);
bool isLimitSwitchTriggered(int motor, int direction);
void moveMotor(int motor, int dir, int steps);
long map(long x, long in_min, long in_max, long out_min, long out_max);
void runFullCalibration();
void testMotors();

void setup() {
  // Start serial and wait for it to be ready
  Serial.begin(115200);
  delay(2000);  // Give serial monitor time to open
  
  // Add debug output
  Serial.println(F("===== STARTUP ====="));
  Serial.println(F("Starting setup..."));
  
  // Configure stepper pins
  pinMode(X_STEP_PIN, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(X_EN_PIN, OUTPUT);
  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(Y_EN_PIN, OUTPUT);

  // Configure Limit Switch pins with pull-down resistors
  pinMode(Y_MIN_SWITCH, INPUT);
  pinMode(Y_MAX_SWITCH, INPUT);
  pinMode(X_MIN_SWITCH, INPUT);
  pinMode(X_MAX_SWITCH, INPUT);
  
  // Enable both motors (active LOW)
  digitalWrite(X_EN_PIN, HIGH);
  digitalWrite(Y_EN_PIN, HIGH);
  
  // Initialize command struct to prevent issues
  resetCommand(&command);
  
  Serial.println(F("Command format examples:"));
  Serial.println(F("  g200,250   (move to X=200, Y=250 coordinate)"));
  Serial.println(F("  x+100      (move X right 100 steps)"));
  Serial.println(F("  x-50       (move X left 50 steps)"));
  Serial.println(F("  y+200      (move Y up 200 steps)"));
  Serial.println(F("  y-75       (move Y down 75 steps)"));
  Serial.println(F("  c          (run full calibration)"));
  Serial.println(F("  a          (auto-calibrate using limit switches)"));
  Serial.println(F("  h          (home to origin)"));
  Serial.println(F("  s          (show current position and limits)"));
  Serial.println(F("  p          (set current position as origin)"));
  Serial.println(F("  t          (test motors - runs a quick test sequence)"));
  
  Serial.println(F("===== READY ====="));

  // Automatically run calibration on startup/reset
  Serial.println(F("Running auto-calibration..."));
  autoCalibrate(); 
  Serial.println(F("Auto-calibration finished after setup."));
}

unsigned long lastDebugTime = 0;
const unsigned long debugInterval = 5000; // 5 seconds

void loop() {
  // Print periodic debug message
  if (DEBUG) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastDebugTime >= debugInterval) {
      lastDebugTime = currentMillis;
      Serial.println(F("System running - waiting for commands..."));
    }
  }
  
  // Read serial input continuously
  if (Serial.available() > 0) {
    readSerialCommand();
    
    // Process commands when they are complete and valid
    if (command.valid) {
      Serial.println(F("Valid command detected!"));
      
      // If a g-command arrives, set the interrupt flag
      if (command.cmd == 'g') {
        shouldInterrupt = true;
        Serial.println(F("G-command received, setting interrupt flag."));
      }
      
      // Process all commands through processCommand
      processCommand(command);
      
      // Reset command buffer after processing
      resetCommand(&command);
    }
  }
}

void processCommand(struct Command cmd) {
  Serial.println(F("==== Processing Command ===="));
  Serial.print(F("Command: "));
  Serial.print(cmd.cmd);
  Serial.print(F(", Arguments: "));
  for (int i = 0; i < cmd.numArgs; i++) {
    Serial.print(cmd.args[i]);
    if (i < cmd.numArgs - 1) {
      Serial.print(F(", "));
    }
  }
  Serial.println();
  
  // We no longer reset the interrupt flag here, 
  // it's handled within moveMotor and goToPosition.
  // shouldInterrupt = false; 
  
  // Default value for args if no argument provided
  int defaultStep = CALIBRATION_STEP_SIZE;
  
  switch (cmd.cmd) {
    case 'x': // X motor command
      {
        // If no argument or argument is 0, use default step size
        int steps = (cmd.numArgs > 0) ? abs(cmd.args[0]) : defaultStep;
        int direction = (cmd.numArgs == 0 || cmd.args[0] >= 0) ? RIGHT : LEFT;
        
        // Reset interrupt flag before starting this specific move
        shouldInterrupt = false;
        
        // Safety bounds check
        if (direction == RIGHT && xCurrent + steps > xMax - BOUNDARY_MARGIN) {
          Serial.print(F("Warning: Movement would exceed X maximum. Limiting to safe value: "));
          steps = (xMax - BOUNDARY_MARGIN) - xCurrent;
          Serial.println(steps);
          if (steps <= 0) {
            Serial.println(F("Cannot move further right."));
            return;
          }
        } else if (direction == LEFT && xCurrent - steps < xMin + BOUNDARY_MARGIN) {
          Serial.print(F("Warning: Movement would exceed X minimum. Limiting to safe value: "));
          steps = xCurrent - (xMin + BOUNDARY_MARGIN);
          Serial.println(steps);
          if (steps <= 0) {
            Serial.println(F("Cannot move further left."));
            return;
          }
        }
        
        // Execute the move
        Serial.print(F("Moving X "));
        Serial.print(direction == RIGHT ? F("RIGHT") : F("LEFT"));
        Serial.print(F(" by "));
        Serial.print(steps);
        Serial.println(F(" steps"));
        
        moveMotor(X_MOTOR, direction, steps);
        
        // Only update position if the move wasn't interrupted
        if (!shouldInterrupt) { 
          if (direction == RIGHT) {
            xCurrent += steps;
          } else {
            xCurrent -= steps;
          }
        }
        
        Serial.println(F("X move command completed"));
        showCurrentPosition();
      }
      break;
      
    case 'y': // Y motor command
      {
        // If no argument or argument is 0, use default step size
        int steps = (cmd.numArgs > 0) ? abs(cmd.args[0]) : defaultStep;
        int direction = (cmd.numArgs == 0 || cmd.args[0] >= 0) ? UP : DOWN;
        
        // Reset interrupt flag before starting this specific move
        shouldInterrupt = false;
        
        // Safety bounds check
        if (direction == UP && yCurrent + steps > yMax - BOUNDARY_MARGIN) {
          Serial.print(F("Warning: Movement would exceed Y maximum. Limiting to safe value: "));
          steps = (yMax - BOUNDARY_MARGIN) - yCurrent;
          Serial.println(steps);
          if (steps <= 0) {
            Serial.println(F("Cannot move further up."));
            return;
          }
        } else if (direction == DOWN && yCurrent - steps < yMin + BOUNDARY_MARGIN) {
          Serial.print(F("Warning: Movement would exceed Y minimum. Limiting to safe value: "));
          steps = yCurrent - (yMin + BOUNDARY_MARGIN);
          Serial.println(steps);
          if (steps <= 0) {
            Serial.println(F("Cannot move further down."));
            return;
          }
        }
        
        // Execute the move
        Serial.print(F("Moving Y "));
        Serial.print(direction == UP ? F("UP") : F("DOWN"));
        Serial.print(F(" by "));
        Serial.print(steps);
        Serial.println(F(" steps"));
        
        moveMotor(Y_MOTOR, direction, steps);
        
        // Only update position if the move wasn't interrupted
        if (!shouldInterrupt) { 
          if (direction == UP) {
            yCurrent += steps;
          } else {
            yCurrent -= steps;
          }
        }
        
        Serial.println(F("Y move command completed"));
        showCurrentPosition();
      }
      break;
      
    case 'g': // Go To position command
      {
        // Require exactly 2 arguments
        if (cmd.numArgs != 2) {
          Serial.println(F("Error: 'g' command requires 2 coordinates (X,Y)"));
          return;
        }
        
        int virtualX = cmd.args[0];
        int virtualY = cmd.args[1];
        
        // Validate coordinates are within virtual range
        if (virtualX < 0 || virtualX > VIRTUAL_WIDTH || virtualY < 0 || virtualY > VIRTUAL_HEIGHT) {
          Serial.println(F("Error: Coordinates out of valid range"));
          Serial.print(F("Valid range: (0,0) to ("));
          Serial.print(VIRTUAL_WIDTH);
          Serial.print(F(","));
          Serial.print(VIRTUAL_HEIGHT);
          Serial.println(F(")"));
          return;
        }
        
        Serial.print(F("Processing goto command - Virtual coordinates: ("));
        Serial.print(virtualX); Serial.print(F(",")); Serial.print(virtualY); Serial.println(F(")"));
        
        // Calculate our actual available space (between safety margins)
        int physicalWidth = (xMax - xMin) - (2 * BOUNDARY_MARGIN);
        int physicalHeight = (yMax - yMin) - (2 * BOUNDARY_MARGIN);
        
        // Map virtual coordinates to physical space
        // Add BOUNDARY_MARGIN to keep within safe area
        int targetX = map(virtualX, 0, VIRTUAL_WIDTH, 
                       xMin + BOUNDARY_MARGIN, 
                       xMax - BOUNDARY_MARGIN);
                       
        int targetY = map(virtualY, 0, VIRTUAL_HEIGHT, 
                       yMin + BOUNDARY_MARGIN, 
                       yMax - BOUNDARY_MARGIN);
        
        Serial.print(F("Mapped to physical coordinates: (")); 
        Serial.print(targetX); Serial.print(F(",")); Serial.print(targetY); Serial.println(F(")"));
        
        // Call goToPosition which handles movement sequence and interrupt flag reset
        goToPosition(targetX, targetY);
      }
      break;
      
    case 'c': // Calibration command
      Serial.println(F("Starting full calibration..."));
      runFullCalibration();
      Serial.println(F("Full calibration complete!"));
      break;
      
    case 'h': // Home command - go to (0,0)
      homeToOrigin();
      break;
      
    case 's': // Status command - show current position and limits
      showCurrentPosition();
      break;
      
    case 'p': // Set current position as (0,0)
      setCurrentPositionAsOrigin();
      break;
      
    case 'a': // Auto-calibration
      autoCalibrate();
      break;
      
    case 't': // Test motors
      testMotors();
      break;
      
    default:
      Serial.print(F("Unknown command: "));
      Serial.println(cmd.cmd);
      break;
  }
  
  Serial.println(F("==== Command Processing Complete ===="));
}

void runFullCalibration() {
  // Set calibration flag
  isCalibrating = true;
  
  Serial.println(F("Starting X axis calibration..."));
  calibrateXAxis();
  
  Serial.println(F("Starting Y axis calibration..."));
  calibrateYAxis();
  
  // After calibration, min/min becomes our 0,0 reference
  // But we're currently at max/max, so update current position
  xCurrent = xMax - xMin;
  yCurrent = yMax - yMin;
  
  // Shift coordinate system to make min/min the origin
  xMax = xMax - xMin;
  yMax = yMax - yMin;
  xMin = 0;
  yMin = 0;
  
  // End calibration mode
  isCalibrating = false;
  
  Serial.println(F("Calibration complete. Moving to safe home position..."));
  // Move to safe home position (min + margin)
  homeToOrigin();
  
  Serial.println(F("Calibration results:"));
  showCurrentPosition();
}

void calibrateXAxis() {
  bool calibratingMin = true;
  bool calibratingMax = false;
  int tempMin = 0;
  int tempMax = 0;
  int tempPos = 0; // Track position relative to starting point of calibration
  
  // Create a local copy of command for calibration
  struct Command localCmd;
  resetCommand(&localCmd);

  Serial.println(F("=== X AXIS CALIBRATION - MIN ==="));
  Serial.println(F("Move LEFT until you hit the minimum limit."));
  Serial.println(F("Use commands like 'x-10', 'x-100'."));
  Serial.println(F("Enter 'm' to mark this position as the MINIMUM X."));

  delayMicroseconds(100);

  while (calibratingMin) {
    if (Serial.available()) {
      readSerialCommand();

      if (command.valid) {
        if (command.cmd == 'x') {
          int steps = (command.numArgs > 0) ? abs(command.args[0]) : CALIBRATION_STEP_SIZE;
          int direction = (command.numArgs == 0 || command.args[0] >= 0) ? RIGHT : LEFT;

          moveMotor(X_MOTOR, direction, steps);
          if (direction == RIGHT) {
            tempPos += steps;
          } else {
            tempPos -= steps;
          }
          
          Serial.print(F("Current Temp X Position: ")); 
          Serial.println(tempPos);
        } 
        else if (command.cmd == 'm') {
          tempMin = tempPos;
          Serial.print(F("Minimum X marked at relative position: ")); 
          Serial.println(tempMin);
          calibratingMin = false;
          calibratingMax = true;
          
          Serial.println(F("\n=== X AXIS CALIBRATION - MAX ==="));
          Serial.println(F("Now move RIGHT until you hit the maximum limit."));
          Serial.println(F("Use commands like 'x+10', 'x+100'."));
          Serial.println(F("Enter 'M' (uppercase) to mark this position as the MAXIMUM X."));
        } 
        else if (command.cmd == 'q') {
          Serial.println(F("Calibration aborted by user."));
          digitalWrite(X_EN_PIN, HIGH);
          digitalWrite(Y_EN_PIN, HIGH);
          isCalibrating = false;
          return;
        }
        
        resetCommand(&command);
      }
    }
    delay(10);
  }

  while (calibratingMax) {
    if (Serial.available()) {
      readSerialCommand();

      if (command.valid) {
        if (command.cmd == 'x') {
          int steps = (command.numArgs > 0) ? abs(command.args[0]) : CALIBRATION_STEP_SIZE;
          int direction = (command.numArgs == 0 || command.args[0] >= 0) ? RIGHT : LEFT;

          moveMotor(X_MOTOR, direction, steps);
          if (direction == RIGHT) {
            tempPos += steps;
          } else {
            tempPos -= steps;
          }
          
          Serial.print(F("Current Temp X Position: ")); 
          Serial.println(tempPos);
        } 
        else if (command.cmd == 'M') {
          tempMax = tempPos;
          Serial.print(F("Maximum X marked at relative position: ")); 
          Serial.println(tempMax);
          calibratingMax = false;
        } 
        else if (command.cmd == 'q') {
          Serial.println(F("Calibration aborted by user."));
          digitalWrite(X_EN_PIN, HIGH);
          digitalWrite(Y_EN_PIN, HIGH);
          isCalibrating = false;
          return;
        }
        
        resetCommand(&command);
      }
    }
    delay(10);
  }

  xMin = tempMin;
  xMax = tempMax;
  xCurrent = tempPos;

  Serial.println(F("=== X Axis Calibration Complete ==="));
  Serial.print(F("X Min: ")); Serial.print(xMin);
  Serial.print(F(" X Max: ")); Serial.print(xMax);
  Serial.print(F(" Range: ")); Serial.println(xMax - xMin);
}

void calibrateYAxis() {
  bool calibratingMin = true;
  bool calibratingMax = false;
  int tempMin = 0;
  int tempMax = 0;
  int tempPos = 0;

  Serial.println(F("=== Y AXIS CALIBRATION - MIN ==="));
  Serial.println(F("Move DOWN until you hit the minimum limit."));
  Serial.println(F("Use commands like 'y-10', 'y-100'."));
  Serial.println(F("Enter 'm' to mark this position as the MINIMUM Y."));

  while (calibratingMin) {
    if (Serial.available()) {
      readSerialCommand();

      if (command.valid) {
        if (command.cmd == 'y') {
          int steps = (command.numArgs > 0) ? abs(command.args[0]) : CALIBRATION_STEP_SIZE;
          int direction = (command.numArgs == 0 || command.args[0] >= 0) ? UP : DOWN;

          moveMotor(Y_MOTOR, direction, steps);
          if (direction == UP) {
            tempPos += steps;
          } else {
            tempPos -= steps;
          }
          
          Serial.print(F("Current Temp Y Position: ")); 
          Serial.println(tempPos);
        } 
        else if (command.cmd == 'm') {
          tempMin = tempPos;
          Serial.print(F("Minimum Y marked at relative position: ")); 
          Serial.println(tempMin);
          calibratingMin = false;
          calibratingMax = true;
          
          Serial.println(F("\n=== Y AXIS CALIBRATION - MAX ==="));
          Serial.println(F("Now move UP until you hit the maximum limit."));
          Serial.println(F("Use commands like 'y+10', 'y+100'."));
          Serial.println(F("Enter 'M' (uppercase) to mark this position as the MAXIMUM Y."));
        } 
        else if (command.cmd == 'q') {
          Serial.println(F("Calibration aborted by user."));
          isCalibrating = false;
          return;
        }
        
        resetCommand(&command);
      }
    }
    delay(10);
  }

  while (calibratingMax) {
    if (Serial.available()) {
      readSerialCommand();

      if (command.valid) {
        if (command.cmd == 'y') {
          int steps = (command.numArgs > 0) ? abs(command.args[0]) : CALIBRATION_STEP_SIZE;
          int direction = (command.numArgs == 0 || command.args[0] >= 0) ? UP : DOWN;

          moveMotor(Y_MOTOR, direction, steps);
          if (direction == UP) {
            tempPos += steps;
          } else {
            tempPos -= steps;
          }
          
          Serial.print(F("Current Temp Y Position: ")); 
          Serial.println(tempPos);
        } 
        else if (command.cmd == 'M') {
          tempMax = tempPos;
          Serial.print(F("Maximum Y marked at relative position: ")); 
          Serial.println(tempMax);
          calibratingMax = false;
        } 
        else if (command.cmd == 'q') {
          Serial.println(F("Calibration aborted by user."));
          isCalibrating = false;
          return;
        }
        
        resetCommand(&command);
      }
    }
    delay(10);
  }

  yMin = tempMin;
  yMax = tempMax;
  yCurrent = tempPos;

  Serial.println(F("=== Y Axis Calibration Complete ==="));
  Serial.print(F("Y Min: ")); Serial.print(yMin);
  Serial.print(F(" Y Max: ")); Serial.print(yMax);
  Serial.print(F(" Range: ")); Serial.println(yMax - yMin);
}

void homeToOrigin() {
  Serial.println(F("Homing to origin (with safety margin)..."));
  
  // Calculate steps needed to reach safe home position
  int targetX = xMin + BOUNDARY_MARGIN;  // Safe X home
  int targetY = yMin + BOUNDARY_MARGIN;  // Safe Y home
  
  // Calculate required movement
  int xSteps = targetX - xCurrent;
  int ySteps = targetY - yCurrent;
  
  // Move to safe home position
  if (xSteps != 0) {
    if (xSteps > 0) {
      moveMotor(X_MOTOR, RIGHT, xSteps);
    } else {
      moveMotor(X_MOTOR, LEFT, abs(xSteps));
    }
    xCurrent = targetX;
  }
  
  if (ySteps != 0) {
    if (ySteps > 0) {
      moveMotor(Y_MOTOR, UP, ySteps);
    } else {
      moveMotor(Y_MOTOR, DOWN, abs(ySteps));
    }
    yCurrent = targetY;
  }
  
  Serial.print(F("Now at safe home position ("));
  Serial.print(xCurrent); Serial.print(F(",")); 
  Serial.print(yCurrent); Serial.println(F(")"));
}

void setCurrentPositionAsOrigin() {
  Serial.println(F("Setting current position as new origin (0,0)"));
  
  // Adjust limits relative to current position becoming (0,0)
  xMax = xMax - xCurrent;
  xMin = xMin - xCurrent;
  yMax = yMax - yCurrent;
  yMin = yMin - yCurrent;
  
  // Set current position to origin
  xCurrent = 0;
  yCurrent = 0;
  
  Serial.println(F("Origin reset complete. New coordinate system:"));
  showCurrentPosition();
}

void showCurrentPosition() {
  Serial.println(F("=== Current Status ==="));
  Serial.print(F("Position: ("));
  Serial.print(xCurrent);
  Serial.print(F(", "));
  Serial.print(yCurrent);
  Serial.println(F(")"));
  
  Serial.print(F("X range: "));
  Serial.print(xMin);
  Serial.print(F(" to "));
  Serial.println(xMax);
  
  Serial.print(F("Y range: "));
  Serial.print(yMin);
  Serial.print(F(" to "));
  Serial.println(yMax);
  
  // Check if within safe boundaries
  if (xCurrent < xMin || xCurrent > xMax || yCurrent < yMin || yCurrent > yMax) {
    Serial.println(F("WARNING: Position outside calibrated range!"));
  }
}

// Check if a limit switch is triggered
bool isLimitSwitchTriggered(int motor, int direction) {
  if (motor == X_MOTOR) {
    if (direction == LEFT && digitalRead(X_MIN_SWITCH)) {
      return true;
    } else if (direction == RIGHT && digitalRead(X_MAX_SWITCH)) {
      return true;
    }
  } else if (motor == Y_MOTOR) {
    if (direction == DOWN && digitalRead(Y_MIN_SWITCH)) {
      return true;
    } else if (direction == UP && digitalRead(Y_MAX_SWITCH)) {
      return true;
    }
  }
  return false;
}

// Move motor (int) in a given direction (UP, DOWN, LEFT, RIGHT) a given number of steps.
void moveMotor(int motor, int dir, int steps) {
  // Fast path for single steps (optimization for Bresenham algorithm)
  if (steps == 1) {
    if (motor == X_MOTOR) {
      // Check for interrupt
      if (shouldInterrupt) {
        Serial.println(F("Movement interrupted"));
        shouldInterrupt = false;
        return;
      }
      
      digitalWrite(X_DIR_PIN, dir);
      digitalWrite(Y_DIR_PIN, dir);
      
      // Only check limit switches if not in calibration mode
      if (!isCalibrating && isLimitSwitchTriggered(X_MOTOR, dir)) {
        Serial.println(F("WARNING: X limit switch triggered - skipping movement"));
        return;  // Skip movement if limit switch is triggered
      }
      
      digitalWrite(X_STEP_PIN, HIGH);
      digitalWrite(Y_STEP_PIN, HIGH);
      delayMicroseconds(STEP_DELAY);
      digitalWrite(X_STEP_PIN, LOW);
      digitalWrite(Y_STEP_PIN, LOW);
      delayMicroseconds(STEP_DELAY);
    } else if (motor == Y_MOTOR) {
      // Check for interrupt
      if (shouldInterrupt) {
        Serial.println(F("Movement interrupted"));
        shouldInterrupt = false;
        return;
      }
      
      digitalWrite(Y_DIR_PIN, dir);
      digitalWrite(X_DIR_PIN, !dir);
      
      // Only check limit switches if not in calibration mode
      if (!isCalibrating && isLimitSwitchTriggered(Y_MOTOR, dir)) {
        Serial.println(F("WARNING: Y limit switch triggered - skipping movement"));
        return;  // Skip movement if limit switch is triggered
      }
      
      digitalWrite(Y_STEP_PIN, HIGH);
      digitalWrite(X_STEP_PIN, HIGH);
      delayMicroseconds(STEP_DELAY);
      digitalWrite(Y_STEP_PIN, LOW);
      digitalWrite(X_STEP_PIN, LOW);
      delayMicroseconds(STEP_DELAY);
    }
    return;
  }
  
  // Original path for multi-step movements
  // X MOTOR STEPPING
  if (motor == X_MOTOR) {
    digitalWrite(X_DIR_PIN, dir);
    digitalWrite(Y_DIR_PIN, dir);

    // Serial.println(F("Stepping X motor..."));
    for (int i = 0; i < steps; i++) {
      // Check for interrupt
      if (shouldInterrupt) {
        Serial.println(F("Movement interrupted"));
        shouldInterrupt = false;
        return;
      }

      // Only check limit switches if not in calibration mode, or every X steps for efficiency
      if (!isCalibrating && (i % CHECK_SWITCH_EVERY_X_STEPS == 0) && isLimitSwitchTriggered(X_MOTOR, dir)) {
        Serial.println(F("WARNING: X-axis limit switch activated! Stopping movement."));
        break;
      }

      // Calculate step delay for acceleration/deceleration
      int currentDelay = STEP_DELAY;
      if (ENABLE_ACCELERATION && steps > 10) {  // Apply to all moves > 10 steps
        // For short moves, reduce the ramp steps proportionally
        int effectiveRampSteps = min(ACCEL_RAMP_STEPS, steps / 2);

        // Accelerate at start
        if (i < effectiveRampSteps) {
          currentDelay = ACCEL_MAX_DELAY - ((ACCEL_MAX_DELAY - ACCEL_MIN_DELAY) * i / effectiveRampSteps);
        }
        // Decelerate at end
        else if (i >= steps - effectiveRampSteps) {
          int stepsFromEnd = steps - i;
          currentDelay = ACCEL_MAX_DELAY - ((ACCEL_MAX_DELAY - ACCEL_MIN_DELAY) * stepsFromEnd / effectiveRampSteps);
        }
      }

      digitalWrite(X_STEP_PIN, HIGH);
      digitalWrite(Y_STEP_PIN, HIGH);
      delayMicroseconds(currentDelay);
      digitalWrite(X_STEP_PIN, LOW);
      digitalWrite(Y_STEP_PIN, LOW);
      delayMicroseconds(currentDelay);

      // Print progress every 100 steps
      if (i % REPORT_EVERY_X_STEPS == 0 && i > 0 && !isCalibrating) {
        int currentStepX = xCurrent + (dir == RIGHT ? (i + 1) : -(i + 1));
        int virtualX = map(currentStepX, xMin + BOUNDARY_MARGIN, xMax - BOUNDARY_MARGIN, 0, VIRTUAL_WIDTH);
        int virtualY = map(yCurrent, yMin + BOUNDARY_MARGIN, yMax - BOUNDARY_MARGIN, 0, VIRTUAL_HEIGHT);
        Serial.print("p");
        Serial.print(virtualX);
        Serial.print(",");
        Serial.println(virtualY);
      }

    }
    // Serial.println(F("X motor move complete"));
  } else if (motor == Y_MOTOR) {
    digitalWrite(Y_DIR_PIN, dir);
    digitalWrite(X_DIR_PIN, !dir);

    // Serial.println(F("Stepping Y motor..."));
    for (int i = 0; i < steps; i++) {
      // Check for interrupt
      if (shouldInterrupt) {
        Serial.println(F("Movement interrupted"));
        shouldInterrupt = false;
        return;
      }

      // Only check limit switches if not in calibration mode, or every X steps for efficiency
      if (!isCalibrating && (i % CHECK_SWITCH_EVERY_X_STEPS == 0) && isLimitSwitchTriggered(Y_MOTOR, dir)) {
        Serial.println(F("WARNING: Y-axis limit switch activated! Stopping movement."));
        break;
      }

      // Calculate step delay for acceleration/deceleration
      int currentDelay = STEP_DELAY;
      if (ENABLE_ACCELERATION && steps > 10) {  // Apply to all moves > 10 steps
        // For short moves, reduce the ramp steps proportionally
        int effectiveRampSteps = min(ACCEL_RAMP_STEPS, steps / 2);

        // Accelerate at start
        if (i < effectiveRampSteps) {
          currentDelay = ACCEL_MAX_DELAY - ((ACCEL_MAX_DELAY - ACCEL_MIN_DELAY) * i / effectiveRampSteps);
        }
        // Decelerate at end
        else if (i >= steps - effectiveRampSteps) {
          int stepsFromEnd = steps - i;
          currentDelay = ACCEL_MAX_DELAY - ((ACCEL_MAX_DELAY - ACCEL_MIN_DELAY) * stepsFromEnd / effectiveRampSteps);
        }
      }

      digitalWrite(Y_STEP_PIN, HIGH);
      digitalWrite(X_STEP_PIN, HIGH);
      delayMicroseconds(currentDelay);
      digitalWrite(Y_STEP_PIN, LOW);
      digitalWrite(X_STEP_PIN, LOW);
      delayMicroseconds(currentDelay);

      // Print progress every 100 steps
      if (i % REPORT_EVERY_X_STEPS == 0 && i > 0 && !isCalibrating) {
        int currentStepY = yCurrent + (dir == UP ? (i + 1) : -(i + 1));
        int virtualX = map(xCurrent, xMin + BOUNDARY_MARGIN, xMax - BOUNDARY_MARGIN, 0, VIRTUAL_WIDTH);
        int virtualY = map(currentStepY, yMin + BOUNDARY_MARGIN, yMax - BOUNDARY_MARGIN, 0, VIRTUAL_HEIGHT);
        Serial.print("p");
        Serial.print(virtualX);
        Serial.print(",");
        Serial.println(virtualY);
      }
    }
    // Serial.println(F("Y motor move complete"));
  } else {
    Serial.print(F("ERROR: Invalid motor specified: "));
    Serial.println(motor);
  }
}

void goToPosition(int targetX, int targetY) {
  // Calculate movements needed
  int deltaX = targetX - xCurrent;
  int deltaY = targetY - yCurrent;
  
  Serial.print(F("Moving to position ("));
  Serial.print(targetX); Serial.print(F(",")); Serial.print(targetY);
  Serial.println(F(")"));
  
  // Reset interrupt flag before starting this specific sequence
  shouldInterrupt = false;
  
  // First move X axis (if needed)
  // Throughout, we broadcast "virtualX" position
  if (deltaX != 0) {
    int direction = (deltaX > 0) ? RIGHT : LEFT;
    int steps = abs(deltaX);
    Serial.print(F("Moving X "));
    Serial.print(direction == RIGHT ? F("RIGHT") : F("LEFT"));
    Serial.print(F(" by ")); Serial.println(steps);
    moveMotor(X_MOTOR, direction, steps);
    
    // If interrupted during X move, stop
    if (shouldInterrupt) { 
        Serial.println(F("goToPosition interrupted during X move."));
        return; 
    }
    // Update position *after* moveMotor completes (assuming no interrupt)
    xCurrent = targetX;
  }
  
  // Then move Y axis (if needed)
  // Throughout, we broadcast "virtualY" position
  if (deltaY != 0) {
    int direction = (deltaY > 0) ? UP : DOWN;
    int steps = abs(deltaY);
    Serial.print(F("Moving Y "));
    Serial.print(direction == UP ? F("UP") : F("DOWN"));
    Serial.print(F(" by ")); Serial.println(steps);
    moveMotor(Y_MOTOR, direction, steps);

    // If interrupted during Y move, stop
    if (shouldInterrupt) { 
        Serial.println(F("goToPosition interrupted during Y move."));
        return; 
    }
    // Update position *after* moveMotor completes (assuming no interrupt)
    yCurrent = targetY;
  }
  
  Serial.print(F("Now at physical position: (")); 
  Serial.print(xCurrent); Serial.print(F(",")); 
  Serial.print(yCurrent); Serial.println(F(")"));
}

// Helper function to map values from one range to another
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void autoCalibrate() {
  // Set calibration flag
  isCalibrating = true;
  
  Serial.println(F("Starting auto-calibration using limit switches..."));
  
  // Find X axis limits
  Serial.println(F("Finding X minimum..."));
  findAxisLimit(X_MOTOR, LEFT, X_MIN_SWITCH);
  
  // Move a small amount back from the limit to set as min
  Serial.println(F("Moving back from X minimum..."));
  moveMotor(X_MOTOR, RIGHT, 50);  // Move 50 steps away from limit
  xMin = xCurrent - 50;  // Set xMin to include the 50 steps offset
  
  Serial.println(F("Finding X maximum..."));
  findAxisLimit(X_MOTOR, RIGHT, X_MAX_SWITCH);
  
  // Move a small amount back from the limit to set as max
  Serial.println(F("Moving back from X maximum..."));
  moveMotor(X_MOTOR, LEFT, 50);  // Move 50 steps away from limit
  xMax = xCurrent + 50;  // Set xMax to include the 50 steps offset
  
  // Verify X travel is within limits
  if (xMax - xMin > MAX_X_TRAVEL) {
    Serial.println(F("ERROR: X axis travel exceeds maximum allowed distance!"));
    Serial.print(F("Measured: ")); Serial.print(xMax - xMin);
    Serial.print(F(" steps. Maximum: ")); Serial.println(MAX_X_TRAVEL);
    isCalibrating = false;
    return;
  }
  
  // Find Y axis limits
  Serial.println(F("Finding Y minimum..."));
  findAxisLimit(Y_MOTOR, DOWN, Y_MIN_SWITCH);
  
  // Move a small amount back from the limit to set as min
  Serial.println(F("Moving back from Y minimum..."));
  moveMotor(Y_MOTOR, UP, 50);  // Move 50 steps away from limit
  yMin = yCurrent - 50;  // Set yMin to include the 50 steps offset
  
  Serial.println(F("Finding Y maximum..."));
  findAxisLimit(Y_MOTOR, UP, Y_MAX_SWITCH);
  
  // Move a small amount back from the limit to set as max
  Serial.println(F("Moving back from Y maximum..."));
  moveMotor(Y_MOTOR, DOWN, 50);  // Move 50 steps away from limit
  yMax = yCurrent + 50;  // Set yMax to include the 50 steps offset
  
  // Verify Y travel is within limits
  if (yMax - yMin > MAX_Y_TRAVEL) {
    Serial.println(F("ERROR: Y axis travel exceeds maximum allowed distance!"));
    Serial.print(F("Measured: ")); Serial.print(yMax - yMin);
    Serial.print(F(" steps. Maximum: ")); Serial.println(MAX_Y_TRAVEL);
    isCalibrating = false;
    return;
  }
  
  // After calibration, min/min becomes our 0,0 reference
  xCurrent = xMax - xMin;
  yCurrent = yMax - yMin;
  
  // Shift coordinate system to make min/min the origin
  xMax = xMax - xMin;
  yMax = yMax - yMin;
  xMin = 0;
  yMin = 0;
  
  // End calibration mode
  isCalibrating = false;
  
  Serial.println(F("Auto-calibration complete. Moving to safe home position..."));
  homeToOrigin();
  
  Serial.println(F("Calibration results:"));
  showCurrentPosition();
}

void findAxisLimit(int motor, int direction, int limitSwitch) {
  const int MIN_STEP_DELAY = STEP_DELAY;  // Fastest speed
  const int MAX_STEP_DELAY = STEP_DELAY * 3;  // Starting speed (slower)
  const int ACCEL_STEPS = 50;  // How many steps to accelerate over
  const int STEP_SIZE = 20;  // Smaller step size for smoother movement
  
  int steps_taken = 0;
  int max_steps = (motor == X_MOTOR) ? MAX_X_TRAVEL : MAX_Y_TRAVEL;
  int current_delay = MAX_STEP_DELAY;
  
  while (!digitalRead(limitSwitch)) {  // While switch is not triggered
    if (steps_taken >= max_steps) {
      Serial.print(F("ERROR: Maximum travel exceeded ("));
      Serial.print(steps_taken);
      Serial.println(F(" steps). Aborting for safety."));
      return;
    }
    
    // Calculate current step delay (basic acceleration)
    if (steps_taken < ACCEL_STEPS) {
      // Accelerating
      current_delay = MAX_STEP_DELAY - ((MAX_STEP_DELAY - MIN_STEP_DELAY) * steps_taken / ACCEL_STEPS);
    }
    
    // Move with current timing
    if (motor == X_MOTOR) {
      digitalWrite(X_DIR_PIN, direction);
      digitalWrite(Y_DIR_PIN, direction);
      
      for (int i = 0; i < STEP_SIZE; i++) {
        digitalWrite(X_STEP_PIN, HIGH);
        digitalWrite(Y_STEP_PIN, HIGH);
        delayMicroseconds(current_delay);
        digitalWrite(X_STEP_PIN, LOW);
        digitalWrite(Y_STEP_PIN, LOW);
        delayMicroseconds(current_delay);
      }
    } else {
      digitalWrite(Y_DIR_PIN, direction);
      digitalWrite(X_DIR_PIN, !direction);
      
      for (int i = 0; i < STEP_SIZE; i++) {
        digitalWrite(Y_STEP_PIN, HIGH);
        digitalWrite(X_STEP_PIN, HIGH);
        delayMicroseconds(current_delay);
        digitalWrite(Y_STEP_PIN, LOW);
        digitalWrite(X_STEP_PIN, LOW);
        delayMicroseconds(current_delay);
      }
    }
    
    steps_taken += STEP_SIZE;
    
    // Update position tracking
    if (motor == X_MOTOR) {
      xCurrent += (direction == RIGHT ? STEP_SIZE : -STEP_SIZE);
    } else {
      yCurrent += (direction == UP ? STEP_SIZE : -STEP_SIZE);
    }
    
    // Small delay between step groups
    delayMicroseconds(50);
  }
  
  Serial.print(F("Limit found after "));
  Serial.print(steps_taken);
  Serial.println(F(" steps"));
}

// Function to test basic motor functionality
void testMotors() {
  Serial.println(F("==== MOTOR TEST STARTING ===="));
  Serial.println(F("This will move each motor a small amount in each direction"));
  Serial.println(F("to verify that wiring and pin settings are correct."));
  
  // Make sure motors are enabled
  digitalWrite(X_EN_PIN, LOW);
  digitalWrite(Y_EN_PIN, LOW);
  
  delay(500);
  
  // Test X motor RIGHT
  Serial.println(F("\n--- Testing X motor RIGHT ---"));
  digitalWrite(X_DIR_PIN, RIGHT);
  digitalWrite(Y_DIR_PIN, RIGHT);
  
  for (int i = 0; i < 20; i++) {
    digitalWrite(X_STEP_PIN, HIGH);
    digitalWrite(Y_STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY * 5); // Slower for testing
    digitalWrite(X_STEP_PIN, LOW);
    digitalWrite(Y_STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY * 5);
  }
  
  delay(1000);
  
  // Test X motor LEFT
  Serial.println(F("--- Testing X motor LEFT ---"));
  digitalWrite(X_DIR_PIN, LEFT);
  digitalWrite(Y_DIR_PIN, LEFT);
  
  for (int i = 0; i < 20; i++) {
    digitalWrite(X_STEP_PIN, HIGH);
    digitalWrite(Y_STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY * 5);
    digitalWrite(X_STEP_PIN, LOW);
    digitalWrite(Y_STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY * 5);
  }
  
  delay(1000);
  
  // Test Y motor UP
  Serial.println(F("\n--- Testing Y motor UP ---"));
  digitalWrite(Y_DIR_PIN, UP);
  digitalWrite(X_DIR_PIN, !UP);
  
  for (int i = 0; i < 20; i++) {
    digitalWrite(Y_STEP_PIN, HIGH);
    digitalWrite(X_STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY * 5);
    digitalWrite(Y_STEP_PIN, LOW);
    digitalWrite(X_STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY * 5);
  }
  
  delay(1000);
  
  // Test Y motor DOWN
  Serial.println(F("--- Testing Y motor DOWN ---"));
  digitalWrite(Y_DIR_PIN, DOWN);
  digitalWrite(X_DIR_PIN, !DOWN);
  
  for (int i = 0; i < 20; i++) {
    digitalWrite(Y_STEP_PIN, HIGH);
    digitalWrite(X_STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY * 5);
    digitalWrite(Y_STEP_PIN, LOW);
    digitalWrite(X_STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY * 5);
  }
  
  Serial.println(F("\n==== MOTOR TEST COMPLETE ===="));
  Serial.println(F("Did you see both motors move in all four directions?"));
  Serial.println(F("If not, check your wiring and pin definitions."));
}
