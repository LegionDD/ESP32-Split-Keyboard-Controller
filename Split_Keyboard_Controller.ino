#include <Adafruit_Keypad_Ringbuffer.h>
#include <Adafruit_Keypad.h>
#include <BleKeyboard.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/**
 * Keyboard Matrix (Keyboard pin to ESP GPIO):
 * C12  P15
 * C10  P2
 * C13  P0
 * C14  P4
 * C15  P16
 * NC
 * C17  P17
 * C18  P5
 * C20  P18
 * R2   P19
 * NC
 * C7   P21
 * C8   P22
 * R3   P23
 * C11  P13
 * R1   P12
 * R4   P14
 * NC
 * C19  P27
 * R5   P26
 * C6   P25
 * NC
 * NC
 * NC
 * C16  P33
 * C9   P32
 * 
 * 15 Cols
 *  5 Rows
 *  
 * OTA enable Pin: P35 (requires external pullup resistor; active low)
 */

const byte ROWS = 5;
const byte COLS = 15;

#define KEY_FN1 200
#define KEY_FN2 201

// two keymap layouts. one without Fn, one with Fn active (the keyboard has two Fn modifier keys)
char keys[ROWS][COLS] = {
  {'0', 'r', '\\', ']', '[', 'o', 'i', 'y', KEY_TAB, 'w', 't', 'u', KEY_LEFT_CTRL, 'p', 'e'},
  {'F', 'f', KEY_RETURN, '\'', ';', 'k', 'j', 'M', KEY_CAPS_LOCK, 's', 'g', 'h', KEY_LEFT_SHIFT, 'l', 'd'},
  {'6', '4', KEY_BACKSPACE, KEY_DELETE, '=', '0', '9', '7', KEY_ESC, '2', '5', '8', '1', '-', '3'},
  {'b', 'c', KEY_RIGHT_SHIFT, KEY_UP_ARROW, '.', 'm', 'n', '0', '0', 'z', 'v', KEY_FN2, 'q', ',', 'x'},
  {' ', '0', '/', KEY_RIGHT_ARROW, KEY_DOWN_ARROW, KEY_RIGHT_GUI, KEY_RIGHT_ALT, '0', 'a', KEY_LEFT_GUI, '0', '0', KEY_FN1, KEY_LEFT_ARROW, KEY_LEFT_ALT}
};

char keysFn1[ROWS][COLS] = {
  {'0', 'r', '\\', ']', '[', 'o', 'i', 'y', KEY_TAB, 'w', 't', 'u', KEY_LEFT_CTRL, 'p', 'e'},
  {'F', 'f', KEY_RETURN, '\'', ';', 'k', 'j', 'M', KEY_CAPS_LOCK, 's', 'g', 'h', KEY_LEFT_SHIFT, 'l', 'd'},
  {KEY_F6, KEY_F4, KEY_BACKSPACE, KEY_DELETE, KEY_F12, KEY_F10, KEY_F9, KEY_F7, '`', KEY_F2, KEY_F5, KEY_F8, KEY_F1, KEY_F11, KEY_F3},
  {'b', 'c', KEY_RIGHT_SHIFT, KEY_PAGE_UP, '.', 'm', 'n', '0', '0', 'z', 'v', KEY_FN2, 'q', ',', 'x'},
  {' ', '0', '/', KEY_END, KEY_PAGE_DOWN, KEY_RIGHT_GUI, KEY_RIGHT_ALT, '0', 'a', KEY_LEFT_GUI, '0', '0', KEY_FN1, KEY_HOME, KEY_LEFT_ALT}
};
                    
byte rowPins[ROWS] = {19, 23, 12, 14, 26};
byte colPins[COLS] = {15, 2, 0, 4, 16, 17, 5, 18, 21, 22, 13, 27, 25, 33, 32};

Adafruit_Keypad splitKeyb = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Adafruit_Keypad splitKeybFn1 = Adafruit_Keypad( makeKeymap(keysFn1), rowPins, colPins, ROWS, COLS);

BleKeyboard bleKeyboard;

bool fn1 = false;
bool fn2 = false;

#define otaPin 35
bool otaMode = false;
const char* ssid = "ssid";
const char* password = "pw";

void setup() {
  Serial.begin(115200); // debugging

  pinMode(otaPin, INPUT);
  if (digitalRead(otaPin) == LOW) { // if OTA button is pressed during power up, enter OTA (over the air programming) mode
    otaMode = true;
  }

  if (otaMode) { // setup OTA
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
  
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
  
    ArduinoOTA.begin();
  
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    splitKeyb.begin();
    splitKeybFn1.begin();
    bleKeyboard.begin();
  }
}

void loop() {
  if (otaMode) { // in OTA Mode only expect the Over The Air update - no keyboard functionality
    ArduinoOTA.handle();
  } else {
    if (fn1 || fn2) { // when Fn (function) key is pressed, use alternate keymap
      splitKeybFn1.tick();
      while (splitKeybFn1.available()){
        keypadEvent e = splitKeybFn1.read();
        processKeybEvent(e);
      }
    } else {
      splitKeyb.tick();
      while (splitKeyb.available()){
        keypadEvent e = splitKeyb.read();
        processKeybEvent(e);
      }
    }
  }
}

void processKeybEvent(keypadEvent e) {
  if(bleKeyboard.isConnected()) {
    if (e.bit.KEY == KEY_FN1) {
      if(e.bit.EVENT == KEY_JUST_PRESSED) fn1 = true;
      else if(e.bit.EVENT == KEY_JUST_RELEASED) fn1 = false;
    } else if (e.bit.KEY == KEY_FN2) {
      if(e.bit.EVENT == KEY_JUST_PRESSED) fn2 = true;
      else if(e.bit.EVENT == KEY_JUST_RELEASED) fn2 = false;
    } else {
      if(e.bit.EVENT == KEY_JUST_PRESSED) bleKeyboard.press(e.bit.KEY);
      else if(e.bit.EVENT == KEY_JUST_RELEASED) bleKeyboard.release(e.bit.KEY);
    }
    
  } else {
    Serial.print((uint8_t) e.bit.KEY);
    if(e.bit.EVENT == KEY_JUST_PRESSED) Serial.println(" pressed");
    else if(e.bit.EVENT == KEY_JUST_RELEASED) Serial.println(" released");
  }
}
