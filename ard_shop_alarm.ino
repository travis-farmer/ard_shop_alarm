#include <SPI.h>
#include <WiFi101.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// Update these with values suitable for your hardware/network.
byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xBA };
IPAddress server(10, 150, 86, 199);
IPAddress ip(192, 168, 0, 28);
IPAddress myDns(192, 168, 0, 1);
// WiFi card example
char ssid[] = WSSID;    // your SSID
char pass[] = WPSWD;       // your SSID Password

WiFiClient wClient;
PubSubClient client(wClient);

String validStates[2] = {"disarmed","armed_away"};
long lastReconnectAttempt = 0;
long currentCode = CODE;
bool stateArmed = false;
bool stateAlert = false;
unsigned long lastTimer = 0UL;

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

  for (int i=22; i <= 42; i++) {
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

  for (int i=22; i <= 42; i++) {
    if (digitalRead(i) == LOW) {
        if (stateArmed == true) {
            stateAlert = true;
        } else {
            // TODO: notification tone
        }
    }
  }

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
}
