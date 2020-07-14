#include <Bounce2.h>
#include "EEPROMAnything.h"
//#include <MemoryFree.h>
#include <NewPing.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define motorPinUp 3
#define motorPinDown 2
#define sonarPongPin 4
#define sonarPingPin 5
#define buttonDownPin 6
#define buttonUpPin 7
#define beeperPin 12

LiquidCrystal_I2C lcd(0x27,8,2);

enum directions { none, up, down, setUp, setDown, checking, moving };

const unsigned int floorOffset = 935-280;
const unsigned int maxSafeHeight = 410;
const unsigned int minSafeHeight = 48;
const unsigned int buttonHoldTime = 2000;
const unsigned int setButtonTimeout = 5000;
const unsigned int sonarTimeout = 50;
const unsigned int screenTimeout = 10000;

directions operationMode = none;
unsigned int topHeight = 390;
unsigned int bottomHeight = 50;
unsigned int targetHeight;
unsigned int lastMeasuredHeight = 0;
unsigned int reportedHeight = 0;
unsigned int heightAccuracy = 1;
unsigned long beepEndTime = 0;
unsigned long buttonPushTime = 0;
unsigned long lastMeasuredTime = 0;
unsigned long screenLastUpdatedTime = 0;

NewPing sonar(sonarPingPin, sonarPongPin);
Bounce upDebounce = Bounce();
Bounce downDebounce = Bounce();

void setup() {
  Serial.begin(115200);
  Serial.println();
  lcd.init();
  resetScreen();
  pinMode(motorPinUp, OUTPUT);
  pinMode(motorPinDown, OUTPUT);
  pinMode(beeperPin, OUTPUT);
  pinMode(sonarPongPin, INPUT);
  pinMode(sonarPingPin, OUTPUT);
  pinMode(buttonUpPin, INPUT_PULLUP);
  pinMode(buttonDownPin, INPUT_PULLUP);
  digitalWrite(motorPinUp, LOW);
  digitalWrite(motorPinDown, LOW);
  upDebounce.attach(buttonUpPin);
  upDebounce.interval(5);
  downDebounce.attach(buttonDownPin);
  downDebounce.interval(5);
  Serial.println(F("================ Deskbot 1.2 ================"));
  loadSettings();
  if (topHeight > maxSafeHeight) {
    topHeight = maxSafeHeight;
    saveSettings();
  }
  if (bottomHeight < minSafeHeight || bottomHeight > topHeight) {
    bottomHeight = minSafeHeight;
    saveSettings();
  }
  reportVitals();
  beep(50);
}

void loop() {
  beep();
  upDebounce.update();
  downDebounce.update();
  if (millis() > screenLastUpdatedTime + screenTimeout) resetScreen();
  if (!upDebounce.read() && !downDebounce.read()) operationMode = none; // if both pressed, then abort

  switch (operationMode) {
    case checking:
      if (millis() < buttonPushTime + buttonHoldTime) {
        if (upDebounce.rose()) {
          operationMode = moving;
          targetHeight = topHeight;
        }
        else if (downDebounce.rose()){
          operationMode = moving;
          targetHeight = bottomHeight;
        }
      } else {
        if (!upDebounce.read()) operationMode = setUp;
        else if (!downDebounce.read()) operationMode = setDown;
        buttonPushTime = millis();
        beep(50);
      }
      break;

    case moving:
      if (!upDebounce.read() || !downDebounce.read()) { // abort if any button pressed
        motorDirection(none); //redundant - safety
        operationMode = none;
        beep(100);
        Serial.println(F("Stop."));
        resetScreen();
        break;
      }
      if (moveDesk(targetHeight)) operationMode = none;
      break;

    case setUp:
      operationMode = none;
      //      if (!upDebounce.read() && !downDebounce.read()) { // abort if both buttons pressed
      //        motorDirection(none); //redundant - safety
      //        operationMode = none;
      //        Serial.println(F("Stop."));
      //        break;
      //      }
      //      if (millis() < buttonPushTime + setButtonTimeout) {
      //        if(upDebounce.fell() || downDebounce.fell()) {
      //          buttonPushTime = millis();
      //          if(upDebounce.fell()) settingHeight = measureHeight() + 1;
      //          if(downDebounce.fell()) settingHeight = measureHeight() - 1;
      //          Serial.println(F("Setting height: ") + String(settingHeight));
      //          moveDesk(settingHeight);
      //        }
      //      } else {
      //        topHeight = settingHeight;
      //        saveSettings();
      //        Serial.println("New top height set: " + String(topHeight));
      //        operationMode = none;
      //      }
      break;

    case setDown:
      operationMode = none;
      break;

    default:
      motorDirection(none);
      if ((upDebounce.fell() && !downDebounce.fell()) || (!upDebounce.fell() && downDebounce.fell())) {
        buttonPushTime = millis();
        operationMode = checking;
        beep(10);
      }
      break;
  }
}

void motorDirection(directions motorDirection) {
  switch (motorDirection) {
    case up:
      digitalWrite(motorPinUp, HIGH);
      digitalWrite(motorPinDown, LOW);
      break;
    case down:
      digitalWrite(motorPinUp, LOW);
      digitalWrite(motorPinDown, HIGH);
      break;
    default:
      digitalWrite(motorPinUp, LOW);
      digitalWrite(motorPinDown, LOW);
      break;
  }
}

bool moveDesk(int newHeight) {
  int currentHeight = measureHeight();
  if (reportedHeight != currentHeight) {
    Serial.println("Current height: " + String(currentHeight) + ". Target height: " + String(newHeight) + ".");
    lcd.setCursor(0,0);
    lcd.print(F("At: "));
    lcd.print(currentHeight);
    lcd.print(F("  "));
    lcd.setCursor(0,1);
    lcd.print(F("To: "));
    lcd.print(newHeight);
    lcd.print(F("  "));
    reportedHeight = currentHeight;
  }
//  if ((currentHeight > maxSafeHeight) || (currentHeight < minSafeHeight)) {
//    beep(500);
//    operationMode = none;
//    motorDirection(none);
//    Serial.println("Out of safe operating range");
//    lcd.setCursor(0,0);
//    lcd.print(F("Out of R"));
//    lcd.setCursor(0,1);
//    lcd.print(F("ange.   "));
//    return false;
//  }

  if ( currentHeight > newHeight + heightAccuracy ) {
    motorDirection(down);
    return false;
  } else if ( currentHeight < newHeight - heightAccuracy ) {
    motorDirection(up);
    return false;
  } else {
    motorDirection(none);
    Serial.println("Target height reached: " + String(newHeight) + "mm.");
    lcd.setCursor(0,0);
    lcd.print(F("Height: "));
    lcd.setCursor(0,1);
    lcd.print(newHeight);
    lcd.print(F("mm       "));
    screenLastUpdatedTime = millis();
    return true;
  }
}

void resetScreen() {
  lcd.setCursor(0,0);
  lcd.print(F("    Desk"));
  lcd.setCursor(0,1);
  lcd.print(F("bot!    "));
  screenLastUpdatedTime = millis();
}

void beep() {
  if ( beepEndTime - millis() > 10000 ) {
    beepEndTime = millis();
  } else if (millis() < beepEndTime) {
    digitalWrite(beeperPin, HIGH);
  } else {
    digitalWrite(beeperPin, LOW);
  }
}

void beep(int beepLength) {
  beepEndTime = millis() + beepLength;
}

int measureHeight() {
  if ((millis() - lastMeasuredTime >= sonarTimeout) || (lastMeasuredTime == 0)) {
    lastMeasuredTime = millis();
    lastMeasuredHeight = sonar.ping_cm() * 10;
  }
  return lastMeasuredHeight;
}

void saveSettings() {
  int eepromAddress = 0;
  eepromAddress += EEPROM_writeAnything(eepromAddress, topHeight);
  eepromAddress += EEPROM_writeAnything(eepromAddress, bottomHeight);
  Serial.println("   Saved Top: " + String(topHeight));
  Serial.println("Saved Bottom: " + String(bottomHeight));
}

void loadSettings() {
  int eepromAddress = 0;
  eepromAddress += EEPROM_readAnything(eepromAddress, topHeight);
  eepromAddress += EEPROM_readAnything(eepromAddress, bottomHeight);
  Serial.println("    Loaded Top: " + String(topHeight) + "mm");
  Serial.println(" Loaded Bottom: " + String(bottomHeight) + "mm");
}

void reportVitals() {
  Serial.print(F("    Top height: "));
  Serial.print(topHeight);
  Serial.println(F("mm"));
  
  Serial.print(F(" Bottom height: "));
  Serial.print(bottomHeight);
  Serial.println(F("mm"));

  Serial.print(F("Current Height: "));
  Serial.print(measureHeight());
  Serial.println(F("mm"));
  
//  Serial.print(F("   Free Memory: "));
//  Serial.print(freeMemory());
//  Serial.println(F(" thingies."));
  Serial.println(F("============================================="));
}
