//////////////////////////////////////////////////
//  LIBRARIES  //
//////////////////////////////////////////////////
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Stepper.h>
#include <Wire.h>
#include <ezButton.h>


//////////////////////////////////////////////////
//  PINS AND PARAMETERS  //
//////////////////////////////////////////////////
#define ALPHABET_SIZE (sizeof(CHARACTERS) / sizeof(struct Character))
#define JOYSTICK_BUTTON_PIN 14       // Connect the joystick button to this pin
#define JOYSTICK_TILT_DELAY 250      // Milliseconds to to delay after detecting joystick tilt
#define JOYSTICK_TILT_THRESHOLD 200  // Adjust this threshold value based on your joystick
#define JOYSTICK_X_PIN A2            // Connect the joystick X-axis to this analog pin
#define JOYSTICK_Y_PIN A1            // Connect the joystick Y-axis to this analog pin
#define LCD_WIDTH 16
#define RESET_Y_STEPS 2500  // The number of steps needed to move the pen holder all the way to the bottom
#define SCALE_X 230         // these are multiplied against the stored coordinate (between 0 and 4) to get the actual number of steps moved
#define SCALE_Y 230         // for example, if this is 230(default), then 230(scale) x 4(max coordinate) = 920 (motor steps)
#define SERVO_DELAY 150     // Milliseconds to delay after moving the servo
#define SERVO_OFF_PAPER_ANGLE 25
#define SERVO_ON_PAPER_ANGLE 80
#define SERVO_PIN 13
#define SPACE (SCALE_X * 5)  // space size between letters (as steps) based on X scale in order to match letter width
#define SPACE_CHARACTER '_'
#define STEPPER_STEPS_PER_REVOLUTION 2048
#define VECTOR_END 192
#define VECTOR_POINTS 14

#define INIT_MSG "Initializing..."     // Text to display on startup
#define MODE_NAME "   LABELMAKER   "   // these are variables for the text which is displayed in different menus.
#define PRINT_CONF "  PRINT LABEL?  "  // try changing these, or making new ones and adding conditions for when they are used
#define PRINTING "    PRINTING    "    // NOTE: this text must be LCD_WIDTH characters or LESS in order to fit on the screen correctly

// structs and enums
struct Character {
  char character;
  byte vectors[VECTOR_POINTS];
  /*
    vector encoding works as follows:
    ones       = y coordinate;
    tens       = x coordinate;
    hundreds   = draw/don't draw ..
    VECTOR_END = end

    Note: Values should not be prefixed with `0` because that would indicate they are octal-literals rather than decimal-literals:
    https://en.cppreference.com/w/cpp/language/integer_literal
  */
};

// Create states to store what the current menu and joystick states are
// Decoupling the state from other functions is good because it means the sensor / screen aren't hardcoded into every single action and can be handled at a higher level
enum State { Edit,
             MainMenu,
             Print,
             PrintConfirmation };

// constants
const struct Character CHARACTERS[] = {
  { SPACE_CHARACTER, { VECTOR_END } },
  { 'A', { 0, 124, 140, 32, 112, VECTOR_END } },
  { 'B', { 0, 104, 134, 132, 2, 142, 140, 100, VECTOR_END } },
  { 'C', { 41, 130, 110, 101, 103, 114, 134, 143, VECTOR_END } },
  { 'D', { 0, 104, 134, 143, 141, 130, 100, VECTOR_END } },
  { 'E', { 40, 100, 104, 144, 22, 102, VECTOR_END } },
  { 'F', { 0, 104, 144, 22, 102, VECTOR_END } },
  { 'G', { 44, 104, 100, 140, 142, 122, VECTOR_END } },
  { 'H', { 0, 104, 2, 142, 44, 140, VECTOR_END } },
  { 'I', { 0, 104, VECTOR_END } },
  { 'J', { 1, 110, 130, 141, 144, VECTOR_END } },
  { 'K', { 0, 104, 2, 142, 140, 22, 144, VECTOR_END } },
  { 'L', { 40, 100, 104, VECTOR_END } },
  { 'M', { 0, 104, 122, 144, 140, VECTOR_END } },
  { 'N', { 0, 104, 140, 144, VECTOR_END } },
  { 'O', { 10, 101, 103, 114, 134, 143, 141, 130, 110, VECTOR_END } },
  { 'P', { 0, 104, 144, 142, 102, VECTOR_END } },
  { 'Q', { 0, 104, 144, 142, 120, 100, 22, 140, VECTOR_END } },
  { 'R', { 0, 104, 144, 142, 102, 22, 140, VECTOR_END } },
  { 'S', { 0, 140, 142, 102, 104, 144, VECTOR_END } },
  { 'T', { 20, 124, 4, 144, VECTOR_END } },
  { 'U', { 4, 101, 110, 130, 141, 144, VECTOR_END } },
  { 'V', { 4, 120, 144, VECTOR_END } },
  { 'W', { 4, 100, 122, 140, 144, VECTOR_END } },
  { 'X', { 0, 144, 4, 140, VECTOR_END } },
  { 'Y', { 4, 122, 144, 22, 120, VECTOR_END } },
  { 'Z', { 4, 144, 100, 140, VECTOR_END } },
  { '0', { 0, 104, 144, 140, 100, 144, VECTOR_END } },
  { '1', { 0, 140, 20, 124, 104, VECTOR_END } },
  { '2', { 4, 144, 142, 102, 100, 140, VECTOR_END } },
  { '3', { 0, 140, 144, 104, 12, 142, VECTOR_END } },
  { '4', { 20, 123, 42, 102, 104, VECTOR_END } },
  { '5', { 0, 140, 142, 102, 104, 144, VECTOR_END } },
  { '6', { 2, 142, 140, 100, 104, 144, VECTOR_END } },
  { '7', { 0, 144, 104, 12, 132, VECTOR_END } },
  { '8', { 0, 140, 144, 104, 100, 2, 142, VECTOR_END } },
  { '9', { 0, 140, 144, 104, 102, 142, VECTOR_END } },
  { '/', { 0, 144, VECTOR_END } },
  { ',', { 0, 111, VECTOR_END } },
  { '-', { 2, 142, VECTOR_END } },
  { '.', { 0, 100, VECTOR_END } },
  { '!', { 0, 100, 1, 104, VECTOR_END } },
  { '?', { 20, 120, 21, 122, 142, 144, 104, VECTOR_END } },
  { '\'', { 23, 124, VECTOR_END } },
  { '&', { 42, 120, 100, 101, 123, 124, 104, 103, 130, 140, VECTOR_END } },
  { '+', { 2, 142, 20, 124, VECTOR_END } },
  { ':', { 21, 121, 23, 123, VECTOR_END } },
  { ';', { 10, 121, 22, 122, VECTOR_END } },
  { '"', { 14, 113, 33, 134, VECTOR_END } },
  { '#', { 10, 114, 34, 130, 41, 101, 3, 143, VECTOR_END } },
  { '(', { 34, 124, 120, 130, VECTOR_END } },
  { ')', { 10, 120, 124, 114, VECTOR_END } },
  { '=', { 1, 141, 43, 103, VECTOR_END } },
  { '@', { 31, 133, 113, 111, 141, 144, 104, 100, 140, VECTOR_END } },
  { '*', { 2, 142, 20, 124, 4, 140, 0, 144, VECTOR_END } },
  { '}', { 0, 140, 144, 104, 100, 12, 113, 33, 132, 31, 111, VECTOR_END } },     // Smiley
  { '~', { 0, 140, 144, 104, 100, 13, 113, 33, 133, 32, 131, 111, 112, 132 } },  // Open mouth Smiley
  { '$', { 20, 142, 143, 134, 123, 114, 103, 102, 120, VECTOR_END } },           // Heart
};

// menu variables
byte characterIndex = 0;               // keep track of which character is currently displayed under the cursor
byte chosenCharacters[LCD_WIDTH - 1];  // keep track of which characters are selected
byte chosenSize = 0;                   // keep track of how many characters have been selected
byte cursorIndex = 0;                  // keeps track of the cursor index (left to right) on the screen
char text[LCD_WIDTH];                  // buffer to hold the text we're plotting, which requires an extra char for '\0'
State currentState = Print;            // The initial value needs to be anything other than `MainMenu`
State previousState;

// hardware variables
bool penOnPaper = false;  // current state of pen on paper
int positionX;
int positionY = RESET_Y_STEPS;  // ensure resetMotors lowers the Y axis all the way to the bottom

// initialize the hardware
ezButton joystickButton(JOYSTICK_BUTTON_PIN);  // https://arduinogetstarted.com/tutorials/arduino-button-library
LiquidCrystal_I2C lcd(0x27, LCD_WIDTH, 2);     // Set the LCD address to 0x27 for a 16x2 display
Servo servo;
Stepper xStepper(STEPPER_STEPS_PER_REVOLUTION, 6, 8, 7, 9);
Stepper yStepper(STEPPER_STEPS_PER_REVOLUTION, 2, 4, 3, 5);


//////////////////////////////////////////////////
//  SETUP  //
//////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);  // initialize serial console so we can output messages

  Serial.println("initializing LCD");
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(INIT_MSG);  // print start up message

  Serial.println("initializing hardware");
  joystickButton.setDebounceTime(50);  // debounce prevents the joystick button from triggering twice when clicked
  servo.attach(SERVO_PIN);             // attaches the servo pin to the servo object
  servo.write(SERVO_OFF_PAPER_ANGLE);  // ensure that the servo is lifting the pen carriage away from the tape
  xStepper.setSpeed(10);               // set x stepper speed (these should stay the same)
  yStepper.setSpeed(12);               // set y stepper speed (^ weird stuff happens when you push it too fast)

  Serial.println("resetting motors");
  resetMotors();
  changeState(MainMenu);
}


////////////////////////////////////////////////
//  LOOP  //
////////////////////////////////////////////////
void loop() {
  joystickButton.loop();  // the loop function must be called in order to call `isPressed`

  int joystickX = analogRead(JOYSTICK_X_PIN);
  int joystickY = analogRead(JOYSTICK_Y_PIN);
  bool joystickDown = joystickY > (512 + JOYSTICK_TILT_THRESHOLD);
  bool joystickLeft = joystickX < (512 - JOYSTICK_TILT_THRESHOLD);
  bool joystickRight = joystickX > (512 + JOYSTICK_TILT_THRESHOLD);
  bool joystickUp = joystickY < (512 - JOYSTICK_TILT_THRESHOLD);

  switch (currentState) {  //state machine that determines what to do with the input controls based on what mode the device is in

    case MainMenu:
      if (previousState != MainMenu) {
        previousState = MainMenu;
        lcd.print(MODE_NAME);
        lcd.setCursor(0, 1);
        lcd.print("      START     ");
        cursorIndex = 5;
      }

      lcd.setCursor(cursorIndex, 1);

      if (millis() % 600 < 400) {  // Blink every 500 ms
        lcd.print(">");
      } else {
        lcd.print(" ");
      }

      if (joystickButton.isPressed()) {  //handles clicking options in text size setting
        changeState(Edit);
      }
      break;

    case Edit:  //in edit mode, joystick directional input adds and removes characters from the string, while up and down changes characters
      //pressing the joystick button will switch the device into the Print Confirmation mode
      if (previousState != Edit) {
        previousState = Edit;
      }
      lcd.setCursor(0, 0);
      lcd.print(":");
      lcd.setCursor(1, 0);
      lcd.print(setText());

      // Check if the joystick is moved up (previous letter) or down (next letter)

      if (joystickUp) {  //UP (previous character)
        if (characterIndex > 0) {
          characterIndex--;
        } else {
          characterIndex = ALPHABET_SIZE - 1;  // move to the end of the available characters
        }
        lcd.print(CHARACTERS[characterIndex].character);
        delay(JOYSTICK_TILT_DELAY);                             // Delay to prevent rapid scrolling
      } else if (joystickDown) {                                //DOWN (next character)
        characterIndex = (characterIndex + 1) % ALPHABET_SIZE;  // `% ALPHABET_SIZE` enables moving back to the first character when at the end
        lcd.print(CHARACTERS[characterIndex].character);
        delay(JOYSTICK_TILT_DELAY);  // Delay to prevent rapid scrolling
      } else {
        if (millis() % 600 < 450) {
          lcd.print(CHARACTERS[characterIndex].character);
        } else {
          lcd.print(" ");
        }
      }

      // Check if the joystick is moved left (backspace) or right (add space)
      if (joystickLeft) {  // LEFT (backspace)
        if (chosenSize > 0) {
          --chosenSize;
          clearDisplay(chosenSize + 1);  // don't clear characters we want to keep to prevent flicker
        }
        delay(JOYSTICK_TILT_DELAY);                       // Delay to prevent rapid multiple presses
      } else if (joystickRight) {                         //RIGHT adds a space or character to the label
        chosenCharacters[chosenSize++] = characterIndex;  //add the current character to the text
        characterIndex = 0;
        delay(JOYSTICK_TILT_DELAY);  // Delay to prevent rapid multiple presses
      }

      if (joystickButton.isPressed()) {
        // Single click: Add character and reset alphabet scroll
        chosenCharacters[chosenSize++] = characterIndex;  //add the current character to the text
        characterIndex = 0;
        changeState(PrintConfirmation);
      }
      break;

    case PrintConfirmation:
      if (previousState != PrintConfirmation) {
        previousState = PrintConfirmation;
        lcd.print(PRINT_CONF);  //print menu text
        lcd.setCursor(0, 1);    // move cursor to the second line
        lcd.print("   YES     NO   ");
        lcd.setCursor(2, 1);
        cursorIndex = 2;
      }

      //the following two if statements help move the blinking cursor from one option to the other.
      if (joystickLeft) {  //left
        lcd.setCursor(0, 1);
        lcd.print("   YES     NO   ");
        lcd.setCursor(2, 1);
        cursorIndex = 2;
        delay(JOYSTICK_TILT_DELAY);
      } else if (joystickRight) {  //right
        lcd.setCursor(0, 1);
        lcd.print("   YES     NO   ");
        lcd.setCursor(10, 1);
        cursorIndex = 10;
        delay(JOYSTICK_TILT_DELAY);
      }

      lcd.setCursor(cursorIndex, 1);

      if (millis() % 600 < 400) {  // Blink every 500 ms
        lcd.print(">");
      } else {
        lcd.print(" ");
      }

      if (joystickButton.isPressed()) {  //handles clicking options in print confirmation
        changeState(cursorIndex == 2 ? Print : Edit);
      }
      break;

    case Print:
      lcd.print(PRINTING);  //update screen

      plotText();
      chosenSize = 0;
      changeState(Edit);
      break;
  }
}


////////////////////////////////////////////////
//  HELPER FUNCTIONS  //
////////////////////////////////////////////////
void changeState(State newState) {  // transition between program states and clear the display
  Serial.print("Changing state ");
  Serial.print(currentState);
  Serial.print("->");
  Serial.println(newState);
  previousState = currentState;
  currentState = newState;
  clearDisplay(0);
}

void clearDisplay(byte columnStart) {
  // Clear the LCD quicker than `lcd.clear` because it doesn't delay for 2 seconds after
  // Iterate over columns, rather than directly print LCD_WIDTH spaces, to trade program space (more code) for less global variable space
  for (byte row = 0; row < 2; ++row) {
    lcd.setCursor(columnStart, row);
    for (byte column = columnStart; column < LCD_WIDTH; ++column)
      lcd.print(" ");
  }
  lcd.setCursor(0, 0);
}

int plotCharacter(struct Character &character, int beginX) {  // this function passes the vectors from a character though the plotLine function to draw it
  Serial.print("character: ");
  Serial.println(character.character);
  for (int i = 0; i < VECTOR_POINTS; i++) {  // iterate through each vector of the character
    byte vector = character.vectors[i];
    if (vector == VECTOR_END) {  // no more vectors in this array
      break;
    }
    bool draw = vector >= 100;
    vector %= 100;               // strip 100s place (draw setting)
    byte vectorX = vector / 10;  // get x ...
    byte vectorY = vector % 10;  // and y

    int endX = beginX + vectorX * SCALE_X;
    int endY = vectorY * SCALE_Y * 3.5;  // we multiply by 3.5 here to equalize the Y output to match X, because the Y lead screw
                                         // covers less distance per-step than the X motor wheel (about 3.5 times less haha)

    Serial.print("Goal: (");
    Serial.print(endX);
    Serial.print(", ");
    Serial.print(endY);
    Serial.print(") Draw: ");
    Serial.println(draw);

    plotLine(endX, endY, draw);
  }
  int ending_space = SPACE;
  if (character.character == 'I') {
    ending_space -= (SCALE_X * 4) / 1.1;
  } else if (character.character == ',') {
    ending_space -= (SCALE_X * 4) / 1.2;
  }
  return ending_space;
}

void plotLine(int newX, int newY, bool drawing) {
  // this function is an implementation of bresenhams line algorithm
  // this algorithm basically finds the slope between any two points, allowing us to figure out how many steps each motor should do to move smoothly to the target
  // in order to do this, we give this function our next X (newX) and Y (newY) coordinates, and whether the pen should be up or down (drawing)
  setPen(drawing);

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

void plotText() {  // breaks up the input by character for plotting
  Serial.print("plot: `");
  Serial.print(setText());
  Serial.println("`");

  int beginX = 0;  // the x coordinate that the next character should start from which may differ from where `positionX` is
  for (byte index = 0; index < chosenSize; ++index) {
    struct Character character = CHARACTERS[chosenCharacters[index]];
    if (character.character == SPACE_CHARACTER) {  // if it's a space, add a space.
      beginX += SPACE;
    } else {
      beginX += plotCharacter(character, beginX);
    }
  }

  plotLine(beginX + SPACE, 0, 0);  // move pen to start location for subsequent plotting
  resetMotors();
}

void resetMotors() {
  const int xPins[4] = { 6, 8, 7, 9 };  // pins for x-motor coils
  const int yPins[4] = { 2, 4, 3, 5 };  // pins for y-motor coils

  setPen(false);
  yStepper.step(0 - positionY);
  positionX = 0;
  positionY = 0;

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

char *setText() {
  // updates the global text array to contain the selected text
  // this array is only used for serial output and when reprinting all the text to the LCD
  // a pointer to the text is returned for convenience
  for (byte index = 0; index < chosenSize; ++index) {
    char character = CHARACTERS[chosenCharacters[index]].character;
    text[index] = character == SPACE_CHARACTER ? ' ' : character;
  }
  text[chosenSize] = '\0';
  return text;
}
