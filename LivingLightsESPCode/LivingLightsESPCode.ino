/**
 * TWO OUTPUTS IRF540 --> LEDS WHITE 4000k pin  14 & LEDS 3200k pin 12
 */

#define VERSION                   "0.1 Beta"

#define BUTTON_ENABLE_WIFIMANAGER 3 //use GPIO 3 = RX ENTER CONFIG
#define LED                       2 //BUILT IN ESP12F BLUE LED
#define WAIT_TO_INIT              1

//ESPECIFICO DEL RGB CONTROLLER
#define GPIO_PIN_4K               14 
#define GPIO_PIN_3K               12 

#define PWM_FREQ                  500 //Hz Was 5000 but I think is too high

/**
 * The following 2 config vars are only for DEBUG purposes
 **/
#define ALWAYS_FORGOT_WIFIMANAGER  0
#define ALWAYS_CLEAN_CONFIG 0

#include <FS.h>                   //this needs to be first, or it all crashes at start up...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson  use version 5.3.1

#include "Ticker.h"
#include <ESP8266NetBIOS.h>
#include <PubSubClient.h>
 
#include "ESP8266mDNS.h"
#include "ArduinoOTA.h"

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);
char* esp_device_id = "_NULL_";     //When Wifi start its changes with mac address
char* ap_ssid = "CONFIG_DEVICE";    //Name of ssid when is in AP mode
char* netbios = "ESP";

//Defaults values...
char server_addr[41] = "10.10.10.90";
char server_port[6] = "1883";
char server_user[41] = "user";
char server_pass[41] = "password";
char mqtt_topic[41] = "home/livingLights";
char token[50] = "6c1ca461f9d2a1b7142dfa34dfaaef813b22ec21";

int light_3K = 0;
int light_4K = 0;

Ticker taskScheduler;

void setup() {
 Serial.begin(115200);
  pinMode (BUTTON_ENABLE_WIFIMANAGER, INPUT_PULLUP);
  pinMode (LED, OUTPUT);
  digitalWrite (LED, LOW);
  
  pinMode (GPIO_PIN_3K, OUTPUT);
  pinMode (GPIO_PIN_4K, OUTPUT);
  analogWriteFreq(PWM_FREQ);
  analogWrite (GPIO_PIN_3K, 0);
  analogWrite (GPIO_PIN_4K, 0);
  
  Serial.println("");
  Serial.println("**********************************************************************");
  Serial.println("* Kricom Solutions (c) 2021                                          *");
  Serial.println("**********************************************************************");
  Serial.println("Booting...");

  delay(1000);
  if (digitalRead(BUTTON_ENABLE_WIFIMANAGER) == LOW) {
    Serial.println("Button pressed... ENTER AP MODE...");
    wifiManager.resetSettings();
  } else {
    /**
     * DELAY on start up... In case of power lose, de Home Access Points needs to boot up first.
     */
    Serial.println("Waiting Routers Wifi to inicialize");
    for(int i=WAIT_TO_INIT; i>=0; i--) {
      Serial.print(i);
      Serial.println(" seg.");
      delay(1000);
    }
 
  }
  
  createDeviceName(); //create device name like DEVICE_XXXXXX
  initWifiManager();  //Init Wifi on AP or client mode
  initConfigTaskScheduler(); //Task Scheduler
  initMqttClient(); //MQTT Publisher and Suscriber

  sprintf(netbios, "IOT_%s", esp_device_id);
  //sprintf(mqtt_topic, "home/rgb%s", esp_device_id);
  NBNS.begin(netbios);

  //Password laprida1235
  ArduinoOTA.setPasswordHash("247d0f4698700004ea8c6e62a2994122");
  ArduinoOTA.begin();
  
  Serial.println("IOT inicialized!!!");
  
}

/**
 * Turn on off light
 */
void manager(bool status) {
  if (status) {
    manager(light_3K, light_4K);
  } else {
    analogWrite (GPIO_PIN_4K, 0);
    analogWrite (GPIO_PIN_3K, 0);
  }
  sendMotta(status);
}

void manager(int l3k, int l4k) {
  //    255 ---- 1023
  //    R ------ x = (R * 1023 / 255)
  analogWrite (GPIO_PIN_4K, l4k * 1023 / 255);
  analogWrite (GPIO_PIN_3K, l3k * 1023 / 255);
  sendMotta(l3k, l4k);
}

void enviarEstado() {
  sendStatusToServer();
}

void loop() {
  ArduinoOTA.handle();
  delay(10);
  if (!client.connected()) {
    reconnect();
  }
  checkStatusWifi();
  client.loop();
  
}
