#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <HX711.h>

// Pin Definitions
const int rpwmPin1 = 9;
const int lpwmPin1 = 13;
const int renPin1 = 8;
const int lenPin1 = 12;

const int rpwmPin2 = 9;
const int lpwmPin2 = 13;
const int renPin2 = 10;
const int lenPin2 = 11;

const int servoPin = 4;
const int ballServoPin = 5;
const int proximitySensorPin = 3;

const int trigPin = 51;
const int echoPin = 50;
const int hx711_dout = 46;
const int hx711_sck = 47;
const int buzzerPin = A4;
const int ledPin = A5;

// Keypad Definitions
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {22, 23, 24, 25};
byte colPins[COLS] = {26, 27, 28, 29};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo ballServo;
Servo mainServo;

HX711 scale;

const float WEIGHT_THRESHOLD = 400.0;
const float CALIBRATION_FACTOR = -330.3333333333333333333333;
const unsigned long INPUT_TIMEOUT = 30000; // 30 seconds timeout for user input

int delayBetweenBalls = 5000;

String inputBuffer = "";

void setup() {
  Serial.begin(9600);  
  Serial1.begin(9600);

  Wire.begin();

  pinMode(rpwmPin1, OUTPUT);
  pinMode(lpwmPin1, OUTPUT);
  pinMode(renPin1, OUTPUT);
  pinMode(lenPin1, OUTPUT);
  pinMode(rpwmPin2, OUTPUT);
  pinMode(lpwmPin2, OUTPUT);
  pinMode(renPin2, OUTPUT);
  pinMode(lenPin2, OUTPUT);
  pinMode(proximitySensorPin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  ballServo.attach(ballServoPin);
  mainServo.attach(servoPin);
  stopMotors();

  scale.begin(hx711_dout, hx711_sck);
  scale.set_scale(CALIBRATION_FACTOR);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Bowling Machine");
  lcd.setCursor(0, 1);
  lcd.print("Ready");

}

void loop() {
  char key = keypad.getKey();
  
  if (key == '*') {
    manualStartMachine();
  } else if (key == '#') {
    stopMachine();
    displayResultAndWait(0, "Machine stopped");
  }

  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      processCommand(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  delay(100);
}

void processCommand(String command) {
  command.trim();
  
  if (command == "CONNECT") {
    displayConnectionStatus(true);
  }
  else if (command.startsWith("START:")) {
    int firstColonIndex = command.indexOf(':');
    int secondColonIndex = command.indexOf(':', firstColonIndex + 1);
    int thirdColonIndex = command.indexOf(':', secondColonIndex + 1);
    
    if (firstColonIndex != -1 && secondColonIndex != -1 && thirdColonIndex != -1) {
      String type = command.substring(firstColonIndex + 1, secondColonIndex);
      int numBalls = command.substring(secondColonIndex + 1, thirdColonIndex).toInt();
      int delayTime = command.substring(thirdColonIndex + 1).toInt();
      
      delayBetweenBalls = delayTime * 1000; // Convert to milliseconds
      
      // Add delay before starting
      lcd.clear();
      lcd.print("Starting in 10s");
      delay(10000);
      
      startMachine(getTypeNumber(type), numBalls);
    }
  } else if (command == "STOP") {
    stopMachine();
  } else if (command == "DISCONNECT") {
    displayConnectionStatus(false);
  }
}

int getTypeNumber(String type) {
  if (type == "fast ball") return 1;
  if (type == "bouncer") return 2;
  if (type == "slow ball") return 3;
  if (type == "inswing") return 4;
  if (type == "outswing") return 5;
  return 3; // Default to slow ball if type is not recognized
}

void manualStartMachine() {
  if (!safetyCheck()) {
    displayResultAndWait(0, "Safety hazard");
    return;
  }

  if (!checkBalls()) {
    displayResultAndWait(0, "Bucket Empty");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select ball type:");
  lcd.setCursor(0, 1);
  lcd.print("1-5, then press *");

  char type = getKeyWithTimeout('1', '5');
  if (type == '\0') {
    displayError("Input timeout");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Balls to bowl:");
  
  int numBalls = getNumberWithTimeout();
  if (numBalls == -1) {
    displayError("Input timeout");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press * to start");
  lcd.setCursor(0, 1);
  lcd.print("bowling");

  if (getKeyWithTimeout('*', '*') == '\0') {
    displayError("Input timeout");
    return;
  }

  startMachine(type - '0', numBalls);
}

void startMachine(int type, int numBalls) {
  if (!safetyCheck()) {
    displayResultAndWait(0, "Safety hazard");
    return;
  }

  if (!checkBalls()) {
    displayResultAndWait(0, "Bucket Empty");
    return;
  }

  switch(type) {
    case 1: fastBall(); break;
    case 2: bouncer(); break;
    case 3: slowBall(); break;
    case 4: inswing(); break;
    case 5: outswing(); break;
    default:
      stopMachine();
      return;
  }

  int ballsBowled = 0;
  for (int i = 0; i < numBalls; i++) {
    if (!safetyCheck()) {
      stopMachine();
      displayResultAndWait(ballsBowled, "Safety hazard");
      return;
    }
    if (bowlBall(type)) {
      ballsBowled++;
    } else {
      stopMachine();
      displayResultAndWait(ballsBowled, "Ball stuck");
      displayDefaultMessage();
      return;
    }
    delay(delayBetweenBalls);
  }

  // Reset the main servo to its initial position (0 degrees) if it was moved
  if (type == 2) {
    moveMainServo(0);
  }

  stopMachine();
  displayResultAndWait(ballsBowled, "Completed");
}

void fastBall() {
  setMotorSpeedRaw(255, 255);
}

void bouncer() {
  moveMainServo(180);
  setMotorSpeedRaw(255, 255);
}

void slowBall() {
  setMotorSpeedRaw(200, 200);
}

void inswing() {
  setMotorSpeedRaw(220, 190);
}

void outswing() {
  setMotorSpeedRaw(190, 220);
}

void setMotorSpeedRaw(int speed1, int speed2) {
  analogWrite(rpwmPin1, speed1);
  analogWrite(rpwmPin2, speed2);
  digitalWrite(renPin1, HIGH);
  digitalWrite(lenPin1, HIGH);
  digitalWrite(renPin2, HIGH);
  digitalWrite(lenPin2, HIGH);
}

void displayDefaultMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bowling Machine");
  lcd.setCursor(0, 1);
  lcd.print("Ready");
}

void displayResultAndWait(int ballsBowled, String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Balls bowled: " + String(ballsBowled));
  lcd.setCursor(0, 1);
  lcd.print(message);
  delay(3000);
  displayDefaultMessage();
}

void displayError(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error:");
  lcd.setCursor(0, 1);
  lcd.print(message);
  delay(2000);
  displayDefaultMessage();
}

bool bowlBall(int type) {
  for (int pos = 0; pos <= 80; pos += 5) {
    ballServo.write(pos);
    delay(15);
  }

  // Return ball servo to starting position
  ballServo.write(0);
  delay(1);
  
  // Wait for the ball to be detected by the proximity sensor
  unsigned long startTime = millis();
  while (digitalRead(proximitySensorPin) == HIGH) {
    if (millis() - startTime > 5000) {  // 5 seconds timeout
      Serial.println("DEBUG: Ball stuck, stopping machine");
      return false;  // Ball stuck
    }
    delay(10);  // Small delay to prevent CPU hogging
  }

  Serial1.println("INFO:Ball bowled");
  Serial.println("DEBUG: Ball bowled");
  return true;
}

void moveMainServo(int angle) {
  mainServo.write(angle);
  delay(1000);
}

void stopMachine() {
  stopMotors();
  ballServo.write(0);
}

void stopMotors() {
  analogWrite(rpwmPin1, 0);
  analogWrite(lpwmPin1, 0);
  analogWrite(rpwmPin2, 0);
  analogWrite(lpwmPin2, 0);
  digitalWrite(renPin1, LOW);
  digitalWrite(lenPin1, LOW);
  digitalWrite(renPin2, LOW);
  digitalWrite(lenPin2, LOW);
}

bool safetyCheck() {
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance <= 200) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
    return false;
  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(ledPin, LOW);
    return true;
  }
}

bool checkBalls() {
  float currentWeight = scale.get_units();
  return currentWeight >= WEIGHT_THRESHOLD;
}

char getKeyWithTimeout(char validMin, char validMax) {
  unsigned long startTime = millis();
  while (millis() - startTime < INPUT_TIMEOUT) {
    char key = keypad.getKey();
    if (key && key >= validMin && key <= validMax) {
      return key;
    }
  }
  return '\0';
}

int getNumberWithTimeout() {
  String inputString = "";
  unsigned long startTime = millis();
  while (millis() - startTime < INPUT_TIMEOUT) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        return inputString.toInt();
      }
      if (key >= '0' && key <= '9') {
        inputString += key;
        lcd.setCursor(0, 1);
        lcd.print(inputString);
      }
    }
  }
  return -1;
}

void displayConnectionStatus(bool connected) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (connected) {
    lcd.print("Connected");
  } else {
    lcd.print("Disconnected");
  }
  delay(1000);
  displayDefaultMessage();
}