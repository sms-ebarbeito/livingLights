bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/**
 * Metodo que hace la magia del inicio, si no hay configuracion se pone en modo AP y solicita info wifi + custom
 */
void initWifiManager() {
  #if defined(ALWAYS_FORGOT_WIFIMANAGER) && ALWAYS_FORGOT_WIFIMANAGER == 1
    Serial.println("ALWAYS_FORGOT_WIFIMANAGER seteado en 1 --> reset WifiManager");
    wifiManager.resetSettings();
  #endif
  sprintf(ap_ssid, "CONFIG_%s", esp_device_id);

  /**
   * readConfigJson //Leer el Json
   */
  #if defined(ALWAYS_CLEAN_CONFIG) && ALWAYS_CLEAN_CONFIG == 1
    Serial.println("ALWAYS_CLEAN_CONFIG seteado en 1 --> format SPIFFS");
    SPIFFS.format();
  #endif
  //read configuration from FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(server_addr, json["server_addr"]);
          strcpy(server_port, json["server_port"]);
          strcpy(server_user, json["server_user"]);
          strcpy(server_pass, json["server_pass"]);
          strcpy(mqtt_topic,  json["mqtt_topic"]);
          strcpy(token,  json["token"]);
          
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  /**
   *  createConfig   //Crear la config
   **/
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_server_addr("server", "MQTT Server", server_addr, 40);
  WiFiManagerParameter custom_server_port("port", "MQTT Port", server_port, 5);
  WiFiManagerParameter custom_server_user("user", "MQTT User", server_user, 40);
  WiFiManagerParameter custom_server_pass("pass", "MQTT Pass", server_pass, 40);
  WiFiManagerParameter custom_mqtt_topic("topic", "MQTT Topic", mqtt_topic, 40);
  WiFiManagerParameter custom_token("token", "token", token, 50);
    
  WiFiManagerParameter custom_label_server("<p>Reporting Server Configuration:</p>");
  wifiManager.addParameter(&custom_label_server);
  wifiManager.addParameter(&custom_server_addr);
  wifiManager.addParameter(&custom_server_port);
  wifiManager.addParameter(&custom_server_user);
  wifiManager.addParameter(&custom_server_pass);
  wifiManager.addParameter(&custom_mqtt_topic);
  wifiManager.addParameter(&custom_token);
  
  wifiManager.setMinimumSignalQuality(5); //descartar redes con menos de 25% de señal
  wifiManager.setRemoveDuplicateAPs(false);
  WiFiManagerParameter custom_text("<p>(c)2018 Kricom</p>");
  wifiManager.addParameter(&custom_text);

  sprintf(netbios, "IOT_%s", esp_device_id);
  WiFi.hostname(netbios);

  

  if (digitalRead(BUTTON_ENABLE_WIFIMANAGER) == LOW) {
    taskScheduler.attach(0.1, blinkLed); //parpadear led
    Serial.println("Button pressed... ENTER AP MODE...");
    wifiManager.resetSettings();
    wifiManager.startConfigPortal(ap_ssid, "");
    taskScheduler.detach(); //dejar de parpadear led
  } else {
    taskScheduler.attach(0.5, blinkLed); //parpadear led
    Serial.println("CONNECTING TO AP...");
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    delay(5000);
    while(WiFi.status() != WL_CONNECTED) {
      WiFi.persistent(false);
      WiFi.mode(WIFI_OFF);   // this is a temporary line, to be removed after SDK update to 1.5.4
      delay(100);
      WiFi.mode(WIFI_STA);
      WiFi.begin();
      delay(5000);
      Serial.print(".");
    }  
    taskScheduler.detach(); //dejar de parpadear led
  }
  
  
  //Fuerza al modo AP y Configuracion
  //wifiManager.startConfigPortal(ap_ssid, "");
  //wifiManager.autoConnect(ap_ssid);
  
  
  digitalWrite(LED, 0);
  
  Serial.println("Conectado al WiFi"); //Imprimimos en Consola que nos conectamos :)
  IPAddress myIP = WiFi.localIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  /**
   * readParametersConfigHtml
   */
  strcpy(server_addr, custom_server_addr.getValue());
  strcpy(server_port, custom_server_port.getValue());
  strcpy(server_user, custom_server_user.getValue());
  strcpy(server_pass, custom_server_pass.getValue());
  strcpy(mqtt_topic, custom_mqtt_topic.getValue());
  strcpy(token, custom_token.getValue());
  if (shouldSaveConfig) {
    /**
     * saveConfig
     */
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["server_addr"] = server_addr;
    json["server_port"] = server_port;
    json["server_user"] = server_user;
    json["server_pass"] = server_pass;
    json["mqtt_topic"] = mqtt_topic;
    json["token"] = token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
}

void createDeviceName() {
  Serial.println("MAC: " + WiFi.macAddress());
  // Crear id de 6 digitos usando los ultimos 3 bloques de MAC address EJ: 84:F3:EB:B7:48:92 crea DEVICE_ID: B74892
  esp_device_id[0] = WiFi.macAddress().charAt(9);
  esp_device_id[1] = WiFi.macAddress().charAt(10);
  esp_device_id[2] = WiFi.macAddress().charAt(12);
  esp_device_id[3] = WiFi.macAddress().charAt(13);
  esp_device_id[4] = WiFi.macAddress().charAt(15);
  esp_device_id[5] = WiFi.macAddress().charAt(16);
  Serial.print("DEVICE_ID: ");
  Serial.println(esp_device_id);
}

void checkStatusWifi() {
  if (WiFi.status() != WL_CONNECTED) {
    taskScheduler.attach(0.5, blinkLed); //parpadear led
    Serial.println("Reconectar");
    WiFi.reconnect();
    taskScheduler.detach();
    digitalWrite(LED, 0);
  }
}


void blinkLed() {
  int state = digitalRead(LED);  // get the current state of GPIO1 pin
  digitalWrite(LED, !state);
}
