#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

const char number_of_rows = 4;
const char number_of_columns = 4;
const int lightSensorPin = A1;  //photoresistor
const int ledPin = 13;    //Led
const int servoPin = A0;  //servo
//light threshold to turn on the LED (value from 0 to 1023)
const int lightThreshold = 500;

char row_pins[number_of_rows] = {11, 10, 9, 8};
char column_pins[number_of_columns] = {7, 6, 5, 4};
char key_array[number_of_rows][number_of_columns] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

boolean doorState = false;
int defaultPassword1 = 1234;
int defaultPassword2 = 9999;

Keypad k = Keypad(makeKeymap(key_array), row_pins , column_pins, number_of_rows, number_of_columns);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;
SoftwareSerial mySerial(2, 3);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;

void setup() {
  pinMode(lightSensorPin, INPUT);
  pinMode(ledPin, OUTPUT);

  lcd.begin(16, 2);
  lcd.backlight();

  myservo.attach(servoPin);
  myservo.write(180);

  // set the data rate for the sensor serial port
  finger.begin(57600);
  lcd.clear();
  lcd.print("Finding Module..");
  lcd.setCursor(0, 1);
  delay(2000);
  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Module Found");
    delay(2000);
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    lcd.clear();
    lcd.print("Module Not Found");
    lcd.setCursor(0, 1);
    lcd.print("Check Connections");
    while (1);
  }
  Serial.begin(9600);
}

//Get id from keypad
uint8_t readIDFromKeypad(void) {
  uint8_t id = 0;
  uint8_t num = 0;
  char key;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter ID#:");

  while (true) {
    key = k.getKey();
    if (key) { // If a key is pressed
      if (key >= '0' && key <= '9') { // Allow up to 3 digits
        // Convert char to integer and add to ID
        id = id * 10 + (key - '0');
        lcd.setCursor(num, 1);
        lcd.print(key); // Display the entered digit
        num++;
      } else if (key == '*') { // Reset entry on '*'
        id = 0;
        num = 0;
        lcd.clear();
        delay(1000);
        lcd.setCursor(0, 0);
        lcd.print("Enter ID#:");
      } else if (key == '#') { // Confirm entry on '#'
        if (id <= 0 || id > 13) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Only support ID#");
          lcd.setCursor(0, 1);
          lcd.print("(1-13)");
          delay(2000); // Extended delay for better readability
          lcd.clear();
          id = 0; // Reset the ID
          num = 0; // Reset the digit count
          lcd.setCursor(0, 0);
          lcd.print("Enter ID#:");
        } else {
          break;
        }
      } else { // Handle invalid key presses
        lcd.setCursor(0, 1);
        lcd.print("Invalid Key  ");
        delay(1000);
        lcd.setCursor(0, 1);
        lcd.print("             "); // Clear invalid key message
      }
    }
  }
  return id;
}

void loop() {
  photoLed();
  if (!doorState) {
    lcd.setCursor(0, 0);
    lcd.print("Smart Lock - GR2");
  }

  char key = k.getKey();
  if (!doorState) {
    if (key == 'D') {
      checkPass();
    }
    if (key == 'A') {
      enroll();
    }
    checkFingerprint();
  } else if (doorState) {
    if (key == '*') {
      closeDoor();
    }
  }
}

//fingerprint method
void checkFingerprint() {
  int result = getFingerprintIDez();
  if (result > 0) {
    openDoor();
    sendToESP8266("Fingerprint");
  }
}

//Enroll fingerprint
void enroll() {
  lcd.clear() ;
  id = readIDFromKeypad();
  if (id != 0) {
    getFingerprintEnroll();
  }
}

//Checking password with default password
void checkPass() {
  int enteredPass = enterPass("Enter Password:");
  if ((enteredPass == defaultPassword1) || (enteredPass == defaultPassword2)) {
    openDoor();
    sendToESP8266("Keypad");
  } else {
    wrongPass();
  }
}

//Type password
int enterPass(String msg) {
  int password = 0;
  int count = 0;
  char key;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
  do {
    key = k.getKey();
    if (key >= '0' && key <= '9') {
      password = password * 10 + (key - '0');
      lcd.setCursor(count, 1);
      lcd.print("*");
      count++;
    } else if (key == '*') {
      password = 0;
      count = 0;
      lcd.clear();
      lcd.print(msg);
    } else if (key == 'C' && count > 0) {
      count--;
      password /= 10;
      lcd.setCursor(count, 1);
      lcd.print(" ");
      lcd.setCursor(count, 1);
    }
  } while (count != 4);
  return password;
}

//Open door
void openDoor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Correct Password");
  myservo.write(0); //servo
  lcd.setCursor(2, 1);
  lcd.print("Door Opened");
  doorState = true;
}

//Close door
void closeDoor() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Door Closed");
  myservo.write(180); //servo
  doorState = false;
  delay(1000);
  lcd.clear();
}

//Wrong password
void wrongPass() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wrong password");
  lcd.setCursor(3, 1);
  lcd.print("Try Again");
  delay(1000);
  lcd.clear();
}

//Photoresistor with LED
void photoLed() {
  //read value from photoresistor
  int sensorValue = analogRead(lightSensorPin);

  //  Serial.println(sensorValue);

  if (sensorValue > lightThreshold) {
    //turn on the LED
    digitalWrite(ledPin, HIGH);
  } else {
    //turn of the LED
    digitalWrite(ledPin, LOW);
  }

  delay(100);
}

//Store fingerprint
uint8_t getFingerprintEnroll()
{
  int p = -1;
  lcd.clear();
  lcd.print("Finger ID#:");
  lcd.print(id);
  lcd.setCursor(0, 1);
  lcd.print("Place Finger");
  delay(2000);
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        lcd.clear();
        lcd.print("Image taken");
        delay(1500);
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println("No Finger");
        delay(1000);
        lcd.clear();
        lcd.print("No Finger Found");
        lcd.setCursor(0, 1);
        lcd.print("Place finger");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        lcd.clear();
        lcd.print("Comm Error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        lcd.clear();
        lcd.print("Imaging Error");
        break;
      default:
        Serial.println("Unknown error");
        lcd.clear();
        lcd.print("Unknown Error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      lcd.clear();
      lcd.print("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      lcd.clear();
      lcd.print("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      lcd.clear();
      lcd.print("Comm Error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      lcd.clear();
      lcd.print("Feature Not Found");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      lcd.clear();
      lcd.print("Feature Not Found");
      return p;
    default:
      Serial.println("Unknown error");
      lcd.clear();
      lcd.print("Unknown Error");
      return p;
  }

  Serial.println("Remove finger");
  lcd.clear();
  lcd.print("Remove Finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  lcd.clear();
  lcd.print("Place Finger");
  lcd.setCursor(0, 1);
  lcd.print("Again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        return;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #"); Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    lcd.clear();
    lcd.print(" Finger Stored!");
    delay(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  }
  else {
    Serial.println("Unknown error");
    return p;
  }
}

int getFingerprintIDez() {

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    return -1;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    return -1;
  }

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Finger Not Found");
    lcd.setCursor(0, 1);
    lcd.print("Try Later");
    delay(1000);
    lcd.clear();
    return -1;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.println(finger.fingerID);
  lcd.clear();
  lcd.print("Found ID #");
  lcd.print(finger.fingerID);
  delay(1000);
  lcd.clear();
  return finger.fingerID;
}

void sendToESP8266(String method) {
  Serial.print("METHOD=");
  Serial.println(method);
}
