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
#define JOYSTICK_BUTTON_PIN 14  // Connect the joystick button to this pin
#define LCD_WIDTH 16
#define LOGO_SCALE_X 15  // Multiplied scale
#define LOGO_VECTOR_SIZE (sizeof(LOGO_VECTOR) / sizeof(LOGO_VECTOR[0]))

#define RESET_Y_STEPS 3500  // The number of steps needed to move the pen holder all the way to the bottom
#define SERVO_DELAY 150     // Milliseconds to delay after moving the servo
#define SERVO_OFF_PAPER_ANGLE 25
#define SERVO_ON_PAPER_ANGLE 80
#define SERVO_PIN 13
#define STEPPER_STEPS_PER_REVOLUTION 2048

#define INIT_MSG "Initializing..."  // Text to display on startup
#define MODE_NAME "    MARK LOGO"   // these are variables for the text which is displayed in different menus.
#define PLOTTING "   PLOTTING..."   //try changing these, or making new ones and adding conditions for when they are used

// enums and structs
enum JoystickButtonState { NEUTRAL,
                           SINGLE_CLICK };

enum MenuState { MainMenu,
                 PlotLogo };

// constants
const byte LOGO_VECTOR[][3] = {
  // the first number is X coordinate, second is Y coordinate, and third is pen up / down (0 = up)
  { 57, 6, 0 },
  { 33, 9, 1 },
  { 60, 15, 0 },
  { 30, 18, 1 },
  { 60, 24, 0 },
  { 30, 27, 1 },
  { 30, 36, 1 },
  { 21, 42, 1 },
  { 21, 75, 1 },
  { 45, 84, 1 },
  { 71, 75, 1 },
  { 71, 42, 1 },
  { 60, 36, 1 },
  { 60, 30, 1 },
  { 60, 48, 0 },
  { 60, 69, 1 },
  { 45, 63, 1 },
  { 30, 69, 1 },
  { 30, 48, 1 },
  { 45, 63, 0 },
  { 45, 42, 1 },
  { 80, 0, 0 }  // Advance forward after drawing logo
};

// menu variables
MenuState currentState = PlotLogo;  // The initial value needs to be anything other than `MainMenu`
MenuState previousState;

// hardware variables
bool penOnPaper = false;  // current state of pen on paper
int positionX;
int positionY = RESET_Y_STEPS;  // ensure resetMotors lowers the Y axis all the way to the bottom
JoystickButtonState joystickButtonState;

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
    joystickButtonState = SINGLE_CLICK;
  });

  Serial.println(F("initializing motors"));
  servo.attach(SERVO_PIN);             // attaches the servo pin to the servo object
  servo.write(SERVO_OFF_PAPER_ANGLE);  // ensure that the servo is lifting the pen carriage away from the tape
  xStepper.setSpeed(10);               // set x stepper speed (these should stay the same)
  yStepper.setSpeed(12);               // set y stepper speed (^ weird stuff happens when you push it too fast)

  changeState(MainMenu);
}


////////////////////////////////////////////////
//  L O O P  //
////////////////////////////////////////////////
void loop() {
  joystickButtonState = NEUTRAL;  // Because callbacks are used to set this value, we need to explicitly clear it before calling tick
  joystickButton.tick();

  switch (currentState) {  // state machine that determines what to do with the input controls based on what mode the device is in
    case MainMenu:
      handleMainMenu();
      break;
    case PlotLogo:
      handlePlotLogo();
      break;
  }
}


////////////////////////////////////////////////
//  STATE FUNCTIONS  //
////////////////////////////////////////////////
void handleMainMenu() {
  if (previousState != MainMenu) {
    previousState = MainMenu;
    resetMotors();
    lcd.print(F(MODE_NAME));
    lcd.setCursor(0, 1);
    lcd.print(F("     >START"));
    lcd.setCursor(5, 1);
    lcd.blink();                                     // blink the `>` at the cursor
  } else if (joystickButtonState == SINGLE_CLICK) {  // handles clicking
    changeState(PlotLogo);
  }
}

void handlePlotLogo() {
  // state does not need to be checked here because this case should execute only once
  previousState = PlotLogo;
  lcd.print(F(MODE_NAME));
  lcd.setCursor(0, 1);
  lcd.print(F(PLOTTING));

  plotLogo();

  changeState(MainMenu);
}


////////////////////////////////////////////////
//  HELPER FUNCTIONS  //
////////////////////////////////////////////////
void changeState(MenuState newState) {  // transition between program states and clear the display
  Serial.print(F("Changing state "));
  Serial.print(currentState);
  Serial.print(F("->"));
  Serial.println(newState);
  previousState = currentState;
  currentState = newState;
  lcd.noBlink();
  clearDisplay(0, 0);
}

void clearDisplay(byte columnStart, byte rowStart) {
  // Clear the LCD quicker than `lcd.clear` because it doesn't delay for 2 seconds after
  // Iterate over columns, rather than directly print LCD_WIDTH spaces, to trade program space (more code) for less global variable space
  for (byte row = rowStart; row < 2; ++row) {
    lcd.setCursor(columnStart, row);
    for (byte column = columnStart; column < LCD_WIDTH; ++column)
      lcd.print(F(" "));
  }
  lcd.setCursor(0, rowStart);
}

void plotLine(int newX, int newY, bool draw) {
  // this function is an implementation of bresenhams line algorithm
  // this algorithm basically finds the slope between any two points, allowing us to figure out how many steps each motor should do to move smoothly to the target
  // in order to do this, we give this function our next X (newX) and Y (newY) coordinates, and whether the pen should be up or down (draw)
  setPen(draw);

  long over;
  long deltaX = newX - positionX;  // calculate the difference between where we are (positionX) and where we want to be (newX)
  long deltaY = newY - positionY;
  int stepX = deltaX > 0 ? -1 : 1;  // this is called a ternary operator, it's basically saying: `if deltaX is greater than 0, then stepX = -1`, otherwise (deltaX is less than or equal to 0), `stepX = 1`
  int stepY = deltaY > 0 ? 1 : -1;
  // the reason one of these ^ is inverted (1/-1) is due to the direction these motors rotate in the system

  deltaX = abs(deltaX);  // normalize the deltaX/deltaY values so that they are positive
  deltaY = abs(deltaY);  // abs() is taking the "absolute value" - basically it removes the negative sign from negative numbers

  // the following if statement checks which change is greater, and uses that to determine which coordinate (x or y) is treated as the rise or the run in the slope calculation
  // we have to do this because technically bresenhams only works for the positive quandrant of the cartesian coordinate grid,
  // so we are just flipping the values around to get the line moving in the correct direction relative to its current position (instead of just up an to the right)
  if (deltaX > deltaY) {
    over = deltaX / 2;
    for (int i = 0; i < deltaX; i++) {  // for however much our current position differs from the target,
      xStepper.step(stepX);             // step in that direction (remember, stepX is always going to be either 1 or -1 from the ternary operator above)
      over += deltaY;
      if (over >= deltaX) {
        over -= deltaX;
        yStepper.step(stepY);
      }
    }
  } else {
    over = deltaY / 2;
    for (int i = 0; i < deltaY; i++) {
      yStepper.step(stepY);
      over += deltaX;
      if (over >= deltaY) {
        over -= deltaY;
        xStepper.step(stepX);
      }
    }
  }
  positionX = newX;  // store new position
  positionY = newY;  // store new position
}

void plotLogo() {  // plots a simplified version of the mark rober logo, stored as coordinates in memory in the LOGO_VECTOR array at the top
  Serial.println(F("MARK LOGO TIME"));
  for (int i = 0; i < LOGO_VECTOR_SIZE; ++i) {
    bool draw = LOGO_VECTOR[i][2] == 1;                  // get draw (0 or 1)
    int endX = LOGO_VECTOR[i][0] * LOGO_SCALE_X;         // get the X for the point we want to hit
    int endY = LOGO_VECTOR[i][1] * LOGO_SCALE_X * 2.75;  // get its Y

    Serial.print(F("Goal: ("));
    Serial.print(endX);
    Serial.print(F(", "));
    Serial.print(endY);
    Serial.print(F(") Draw: "));
    Serial.println(draw);

    plotLine(endX, endY, draw);  // use our plotLine function to head to that X and Y position, the third value is the pen up/down.
  }
}


void resetMotors() {
  const int xPins[4] = { 6, 8, 7, 9 };  // pins for x-motor coils
  const int yPins[4] = { 2, 4, 3, 5 };  // pins for y-motor coils

  setPen(false);
  if (positionY > 0) {
    Serial.println(F("resetting motors"));
    lcd.print(F("Homing Y-Axis"));
    yStepper.step(-positionY);
    positionY = 0;
    clearDisplay(0, 0);
  }
  positionX = 0;

  for (int i = 0; i < 4; i++) {  // deactivates all the motor coils
    digitalWrite(xPins[i], 0);   // picks each motor pin and drops voltage to 0
    digitalWrite(yPins[i], 0);
  }
}

void setPen(bool toPaper) {  // used to handle lifting or lowering the pen on to the tape
  if (toPaper != penOnPaper) {
    servo.write(toPaper ? SERVO_ON_PAPER_ANGLE : SERVO_OFF_PAPER_ANGLE);  // actuate the servo to either position
    delay(SERVO_DELAY);                                                   // gives the servo time to move before jumping into the next action
    penOnPaper = toPaper;                                                 // store the previous state
  }
}
