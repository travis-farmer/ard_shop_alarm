#include <SPI.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include "Adafruit_Keypad.h"
#include <Adafruit_NeoPixel.h>
#define PIN        41 // On Trinket or Gemma, suggest changing this to 1

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS 10 // Popular NeoPixel ring size

// Update these with values suitable for your hardware/network.
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xBA };
IPAddress server(10, 150, 86, 199);
IPAddress ip(192, 168, 0, 28);
IPAddress myDns(192, 168, 0, 1);
// WiFi card example
char ssid[] = WSSID;    // your SSID
char pass[] = WPSWD;       // your SSID Password

const byte ROWS = 4; // rows
const byte COLS = 4; // columns
//define the symbols on the buttons of the keypads
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {33, 34, 35, 36}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {37, 38, 39, 40}; //connect to the column pinouts of the keypad
//initialize an instance of class NewKeypad
Adafruit_Keypad customKeypad = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

WiFiClient wClient;
PubSubClient client(wClient);

String validStates[2] = {"disarmed","armed_away"};
long lastReconnectAttempt = 0;
long currentCode = CODE;
bool stateArmed = false;
bool stateAlert = false;
unsigned long lastTimer = 0UL;
String keyBuffer = "";

boolean reconnect() {
  if (client.connect("arduinoClient", "mqtt_devices", "10994036")) {
    client.subscribe("alarmdecoder/panel/set");


  }
  return client.connected();
}

void alarmSetState(String setState, long setCode) {
    if (setCode == currentCode) {
        if (setState == "ARM_AWAY") {
            stateArmed = true;
        } else if (setState == "DISARM") {
            stateArmed = false;
            stateAlert = false;
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
  DynamicJsonDocument doc(1024);
  String tmpTopic = topic;
  char tmpStr[length+1];
  for (int x=0; x<length; x++) {
    tmpStr[x] = (char)payload[x]; // payload is a stream, so we need to chop it by length.
  }
  tmpStr[length] = 0x00; // terminate the char string with a null

  if (tmpTopic == "alarmdecoder/panel/set") {deserializeJson(doc, tmpStr); alarmSetState(doc["action"],doc["code"]); }


}


void setup() {
    pixels.begin();
    pixels.clear();
    pixels.show();
  //Serial.begin(9600);
  pinMode(13,OUTPUT);
  digitalWrite(13,LOW);
  client.setServer(server, 1883);
  client.setCallback(callback);
  WiFi.setPins(10,7,5);
  int status = WiFi.begin(ssid, pass);
  if ( status != WL_CONNECTED) {
    //Serial.println("Couldn't get a wifi connection");
    while(true);
  }
  // print out info about the connection:
  else {
    //Serial.println("Connected to network");
    //IPAddress ip = WiFi.localIP();
    //Serial.print("My IP address is: ");
    //Serial.println(ip);
  }
  delay(1500);
  lastReconnectAttempt = 0;

  for (int i=22; i <= 32; i++) {
    pinMode(i,INPUT_PULLUP);
  }
}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected

    client.loop();
  }

  for (int i=22; i <= 32; i++) {
    if (digitalRead(i) == LOW) {
        if (stateArmed == true) {
            pixels.setPixelColor(i-22, pixels.Color(255, 0, 0));


        } else {
            // TODO: notification tone
            pixels.setPixelColor(i-22, pixels.Color(0, 0, 0));
        }
    } else {
        if (stateArmed == true) {
            pixels.setPixelColor(i-22, pixels.Color(0, 255, 0));
        } else {
            pixels.setPixelColor(i-22, pixels.Color(0, 0, 255));
        }
    }
  }
  pixels.show();

  if (millis() - lastTimer >= 500) {
    lastTimer = millis();

    // publish current state
    String strState = "";
    if (stateArmed == true) {
        digitalWrite(13,HIGH);
        strState = "armed_away";
    } else {
        digitalWrite(13,LOW);
        strState = "disarmed";
    }

    char sz[32];
    strState.toCharArray(sz, 32);
    client.publish("alarmdecoder/panel",sz);
  }
    customKeypad.tick();

    while(customKeypad.available()){
        keypadEvent e = customKeypad.read();
        Serial.print((char)e.bit.KEY);
        if(e.bit.EVENT == KEY_JUST_RELEASED) {
            char tmpChar = (char)e.bit.KEY;
            switch(tmpChar) {
            case 'A':
                //arm alarm
                alarmSetState("ARM_AWAY", keyBuffer);
                keyBuffer = "";
                break;
            case 'B':
                // backspace
                keyBuffer.remove(keyBuffer.length()-1)
                break;
            case 'C':
                // cancel
                keyBuffer = "";

                break;
            case 'D':
                //disarm alarm
                alarmSetState("DISARM", keyBuffer);
                break;
            default:
                //all other keys sent to String
                keyBuffer += tmpChar;
                break;
            }
        }
    }


}
