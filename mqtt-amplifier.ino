/*
 * YAMAHA VERSTERKER
 * Remote controll with MQTT
 * 
 * Renze Nicolai 2017
 * Tkkrlab
 * 
 * MIT license
 * 
 */

/* CONSTANTS */

//IR values
#define MAGIC          0x5EA10000
#define KEY_POWER      0xF807
#define KEY_CD         0xA857
#define KEY_AUX        0xE817
#define KEY_TAPE1      0x18E7
#define KEY_TAPE2      0x9867
#define KEY_PHONO      0x28D7
#define KEY_VOLUMEUP   0x58A7
#define KEY_VOLUMEDOWN 0xD827

//MQTT values
#define WLAN_SSID       "<ssid>"
#define WLAN_PASS       "<password>"
#define MQTT_SERVER      "10.42.1.2"
#define MQTT_SERVERPORT  1883
#define MQTT_USERNAME    ""
#define MQTT_KEY         ""

#define DEVICE_NAME      "tkkrlab/amp"

/* INCLUDES */
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/* OBJECTS */
IRsend irsend(4); //D2

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

Adafruit_MQTT_Publish volumePub = Adafruit_MQTT_Publish(&mqtt, DEVICE_NAME "/volume");
Adafruit_MQTT_Subscribe rawcommandSub = Adafruit_MQTT_Subscribe(&mqtt, DEVICE_NAME "/raw");
Adafruit_MQTT_Subscribe volumeSub = Adafruit_MQTT_Subscribe(&mqtt, DEVICE_NAME "/volume");
Adafruit_MQTT_Subscribe modeSub = Adafruit_MQTT_Subscribe(&mqtt, DEVICE_NAME "/mode");

/* VARIABLES */
uint8_t volume = 0;

bool statePower = false;
bool stateTape1 = false;
bool stateTape2 = false;

/* FUNCTIONS */
void setup()
{
  irsend.begin();
  Serial.begin(115200);
  Serial.println("Yamaha amplifier MQTT controller");
  power(true);
  resetVolume();
  connection();
  rawcommandSub.setCallback(cbRaw);
  volumeSub.setCallback(cbVolume);
  modeSub.setCallback(cbMode);
  mqtt.subscribe(&rawcommandSub);
  mqtt.subscribe(&volumeSub);
  mqtt.subscribe(&modeSub);
}

void resetVolume() {
  Serial.print("Resetting volume");
  for (uint8_t i = 0; i<100; i++) {
    Serial.println(i);
    ir(KEY_VOLUMEDOWN);
  }
}

void cbRaw(uint32_t value) {
  Serial.println("Raw command: "+String(value));
  ir(value);
}
void cbVolume(uint32_t value) {
  uint8_t oldVolume = volume;
  volume = value;
  if (volume>100) {
    volume = 100;
  }

  uint8_t change = volume-oldVolume;
  bool direction = true;
  if (volume<oldVolume) {
    change = oldVolume - volume;
    direction = false;
  }
  
  Serial.println("Volume change: "+String(change)+", direction: "+String(direction));

  uint32_t key = KEY_VOLUMEDOWN;
  if (direction) key=KEY_VOLUMEUP;

  for (uint8_t i = 0; i<change; i++) {
    ir(key);
    delay(48);
  }

  Serial.println("Done setting volume.");
  
}

void tape1(bool i) {
  if (stateTape1!=i) {
    ir(KEY_TAPE1);
    stateTape1 = i;
  }
}

void tape2(bool i) {
  if (stateTape2!=i) {
    ir(KEY_TAPE2);
    stateTape2 = i;
  }
}

void power(bool i) {
  if (statePower!=i) {
    Serial.println("Power changed to "+String(i));
    ir(KEY_POWER);
    delay(100);
    statePower = i;
  }
}

void cbMode(uint32_t value) {
  power(value&0x01);
  if (statePower) {
    tape1(value&0x02);
    tape2(value&0x04);
  }
  if (value&0x08) {
    ir(KEY_AUX);
  } else {
    ir(KEY_CD);
  }
}

bool connection() {
  uint8_t timeout = 10;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connecting to network...");
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      timeout--;
      if (timeout<0) return false;
    }
    Serial.println();
    Serial.println("WiFi connected ["+ String(WiFi.localIP()) + "]");
  }
  timeout = 10;
  int8_t ret;
  if (mqtt.connect() != 0) {
    Serial.println("[MQTT] Connecting to server...");
    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying...");
       mqtt.disconnect();
       delay(1000);
       timeout--;
       if (timeout<0) return false;
    }
  }
  return true;
}

void print_command_list() {
  Serial.println("");
  Serial.println("Available commands:");
  Serial.println("p = POWER");
  Serial.println("c = CD");
  Serial.println("a = AUX");
  Serial.println("u = VOLUME UP");
  Serial.println("d = VOLUME DOWN");
  Serial.println("1 = TAPE MONITOR 1");
  Serial.println("2 = TAPE MONITOR 2");
}

void ir(uint32_t key) {
  irsend.sendNEC(MAGIC + key, 32);
  delay(2);
}

void serial() {
  if (Serial.available()>0) {
    char cmd = Serial.read();
    uint16_t key = 0;
    switch (cmd) {
      case 'p':
        key = KEY_POWER;
        break;
      case 'c':
        key = KEY_CD;
        break;
      case 'a':
        key = KEY_AUX;
        break;
      case 'u':
        key = KEY_VOLUMEUP;
        break;
      case 'd':
        key = KEY_VOLUMEDOWN;
        break;
      case '1':
        key = KEY_TAPE1;
        break; 
      case '2':
        key = KEY_TAPE2;
        break;
      default:
        Serial.println("ERROR");
        return;
    }
    Serial.println("OK");
    ir(key);
  }
}

void loop() {
  serial();
  if (connection()) {
    Adafruit_MQTT_Subscribe *subscription;
    mqtt.processPackets(10000);
  }
}
