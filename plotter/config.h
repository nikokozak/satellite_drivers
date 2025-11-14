#ifndef CONFIG_H
#define CONFIG_H

// Pin Definitions
#define X_MOTOR 1
#define Y_MOTOR 2
#define LEFT 0
#define RIGHT 1
#define UP 1
#define DOWN 0

// Limit Switches
#define Y_MIN_SWITCH A3
#define Y_MAX_SWITCH A2
#define X_MIN_SWITCH A1
#define X_MAX_SWITCH A0

// X Motor (Driver 1)
#define X_STEP_PIN 9
#define X_DIR_PIN 8
#define X_EN_PIN 10

// Y Motor (Driver 2)
#define Y_STEP_PIN 5
#define Y_DIR_PIN 6
#define Y_EN_PIN 7

// Movement parameters
#define STEP_DELAY 100   // Microseconds between steps at full speed (lower = faster)
#define STEPS_PER_MOVE 4000 // How many steps to move in each direction (Consider if still needed)
#define CALIBRATION_STEP_SIZE 10 // Default step size for calibration if no amount specified
#define BOUNDARY_MARGIN 50 // Safety margin from limits (in steps)

// Acceleration parameters
#define ENABLE_ACCELERATION true  // Set to false to disable acceleration/deceleration
#define ACCEL_RAMP_STEPS 100      // Number of steps to accelerate/decelerate over
#define ACCEL_MIN_DELAY 100       // Full speed delay (microseconds) - same as STEP_DELAY
#define ACCEL_MAX_DELAY 500       // Starting/ending speed delay (microseconds) - slower = gentler

// Movement Reporting (how often/steps do we report back our coordinates to the host)
#define REPORT_EVERY_X_STEPS 300 // How often to report back to host

// Safety checking
#define CHECK_SWITCH_EVERY_X_STEPS 5 // How often to check limit switches during movement

// Virtual coordinate system
#define VIRTUAL_WIDTH 2000   // Virtual coordinate system width
#define VIRTUAL_HEIGHT 1500  // Virtual coordinate system height

// Physical limits (may need adjustment after calibration)
#define MAX_X_TRAVEL 10500  // Maximum allowed X travel in steps
#define MAX_Y_TRAVEL 7500   // Maximum allowed Y travel in steps

// Command Buffer Size
#define BUFFER_SIZE 32

// Debug flag
#define DEBUG false // Set to true for verbose serial output

#endif // CONFIG_H 
