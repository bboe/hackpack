//////////////////////////////////////////////////
//  LIBRARIES  //
//////////////////////////////////////////////////
#include <LiquidCrystal_I2C.h>
#include <OneButton.h>
#include <Servo.h>
#include <Stepper.h>
#include <Wire.h>


//////////////////////////////////////////////////
//  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
#define JOYSTICK_BUTTON_PIN 14       // Connect the joystick button to this pin
#define JOYSTICK_TILT_THRESHOLD 200  // Adjust this threshold value based on your joystick
#define JOYSTICK_X_PIN A2            // Connect the joystick X-axis to this analog pin
#define JOYSTICK_Y_PIN A1            // Connect the joystick Y-axis to this analog pin
#define LCD_WIDTH 16
#define SERVO_DELAY 150  // Milliseconds to delay after moving the servo
#define SERVO_OFF_PAPER_ANGLE 25
#define SERVO_ON_PAPER_ANGLE 80
#define SERVO_PIN 13
#define SPEED_SCALER 4  // Number of times to run the movement, alters speed, keep low-ish so you don't get locked into its loop forever
#define STEP_SIZE_X 1
#define STEP_SIZE_Y 3
#define STEPPER_STEPS_PER_REVOLUTION 2048

#define BOTTOM_TEXT " CLICK TO DRAW"  // try changing these, or making new ones and adding conditions for when they are used
#define INIT_MSG "Initializing..."    // Text to display on startup
#define MODE_NAME "  SKETCH MODE"     // these are variables for the text which is displayed in different menus.

// enums and structs
enum JoystickButtonState { NEUTRAL,
                           SINGLE_CLICK };

struct JoystickState {
  bool down;
  bool left;
  bool right;
  bool up;
  JoystickButtonState buttonState;
};

// hardware variables
bool penOnPaper = false;  // current state of pen on paper
struct JoystickState joystickState;

// initialize the hardware
LiquidCrystal_I2C lcd(0x27, LCD_WIDTH, 2);  // Set the LCD address to 0x27 for a 16x2 display
OneButton joystickButton;
Servo servo;
Stepper xStepper(STEPPER_STEPS_PER_REVOLUTION, 6, 8, 7, 9);
Stepper yStepper(STEPPER_STEPS_PER_REVOLUTION, 2, 4, 3, 5);


//////////////////////////////////////////////////
//  S E T U P  //
//////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);  // initialize serial console so we can output messages

  Serial.println(F("initializing LCD"));
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(F(INIT_MSG));  // print start up message

  // set up joystick callbacks
  // Lambda functions are used here because we don't need complex logic in the callbacks
  joystickButton.setup(JOYSTICK_BUTTON_PIN, INPUT_PULLUP, true);
  joystickButton.attachClick([]() {
    joystickState.buttonState = SINGLE_CLICK;
  });

  Serial.println(F("initializing motors"));
  servo.attach(SERVO_PIN);             // attaches the servo pin to the servo object
  servo.write(SERVO_OFF_PAPER_ANGLE);  // ensure that the servo is lifting the pen carriage away from the tape
  xStepper.setSpeed(10);               // set x stepper speed (these should stay the same)
  yStepper.setSpeed(12);               // set y stepper speed (^ weird stuff happens when you push it too fast)

  Serial.println(F("resetting motors"));
  resetMotors();

  clearDisplay();
  lcd.print(F(MODE_NAME));  // top line of screen text
  lcd.setCursor(0, 1);
  lcd.print(F(BOTTOM_TEXT));  // bottom line of screen text
}


////////////////////////////////////////////////
//  L O O P  //
////////////////////////////////////////////////
void loop() {
  joystickState.buttonState = NEUTRAL;  // Because callbacks are used to set this value, we need to explicitly clear it before calling tick
  joystickButton.tick();

  if (joystickState.buttonState == SINGLE_CLICK) {
    setPen(!penOnPaper);
    return;  // Ignore x/y movement when pressing the button
  }

  getJoystickState();

  for (int i = 0; i < SPEED_SCALER; ++i) {
    if (joystickState.down)
      yStepper.step(-STEP_SIZE_Y);
    else if (joystickState.up)
      yStepper.step(STEP_SIZE_Y);

    // Up/down and left/right are mutually exclusive but x and y movement can occur together
    if (joystickState.left)
      xStepper.step(STEP_SIZE_X);
    else if (joystickState.right)
      xStepper.step(-STEP_SIZE_X);
  }
}

////////////////////////////////////////////////
//  HELPER FUNCTIONS  //
////////////////////////////////////////////////
void clearDisplay() {
  // Clear the LCD quicker than `lcd.clear` because it doesn't delay for 2 seconds after
  for (byte row = 0; row < 2; ++row) {
    lcd.setCursor(0, row);
    lcd.print(F("                "));
  }
  lcd.setCursor(0, 0);
}

void getJoystickState() {
  int joystickX = analogRead(JOYSTICK_X_PIN);
  int joystickY = analogRead(JOYSTICK_Y_PIN);
  joystickState.down = joystickY > (512 + JOYSTICK_TILT_THRESHOLD);
  joystickState.left = joystickX < (512 - JOYSTICK_TILT_THRESHOLD);
  joystickState.right = joystickX > (512 + JOYSTICK_TILT_THRESHOLD);
  joystickState.up = joystickY < (512 - JOYSTICK_TILT_THRESHOLD);
}

void resetMotors() {
  const int xPins[4] = { 6, 8, 7, 9 };  // pins for x-motor coils
  const int yPins[4] = { 2, 4, 3, 5 };  // pins for y-motor coils

  for (int i = 0; i < 4; i++) {  // deactivates all the motor coils
    digitalWrite(xPins[i], 0);   // picks each motor pin and drops voltage to 0
    digitalWrite(yPins[i], 0);
  }
  setPen(false);
}

void setPen(bool toPaper) {  // used to handle lifting or lowering the pen on to the tape
  if (toPaper != penOnPaper) {
    servo.write(toPaper ? SERVO_ON_PAPER_ANGLE : SERVO_OFF_PAPER_ANGLE);  // actuate the servo to either position
    delay(SERVO_DELAY);                                                   // gives the servo time to move before jumping into the next action
    penOnPaper = toPaper;                                                 // store the previous state
  }
}
