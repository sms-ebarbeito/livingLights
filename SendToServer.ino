

void callback(char* topic, byte* payload, unsigned int length);

void initMqttClient() {

  client.setServer(server_addr, (String(server_port)).toInt() );
  client.setCallback(callback);
  reconnect();
  

}
void sendStatusToServer() {
    
    char msg[200];
    snprintf (msg, 200, "{ \"clientId\" : \"ESP-%s\", \"3K\" : %d, \"4K\" : %d }", esp_device_id, light_3K, light_4K);
    
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(mqtt_topic, msg);

}

void sendToServerInit() {
    char msg[60];
    snprintf (msg, 60, "{ \"clientId\" : \"ESP-%s\", \"message\" : \"Device init ok\" }", esp_device_id);
    
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(mqtt_topic, msg);
}

void sendMotta(bool status) {
  char topic_temp[80] = "";
  snprintf (topic_temp, 80, "%s/get", mqtt_topic);
  Serial.print("Send to topic ");
  Serial.println(topic_temp);
  
  char value[20] = "";
  if (status) {
    client.publish(topic_temp, "ON");
  } else {
    client.publish(topic_temp, "OFF");
  }
  
}

void sendMotta(int l3k, int l4k) {
  char topic_temp[80] = "";
  snprintf (topic_temp, 80, "%s/getRGB", mqtt_topic);
  Serial.print("Send to topic ");
  Serial.print(topic_temp);
  
  char value[20] = "";
  snprintf (value, 20, "%d,%d", l3k, l4k);

  Serial.print(" value 3K: ");
  Serial.print(l3k);
  Serial.print(", 4K: ");
  Serial.print(l4k);
  
  client.publish(topic_temp, value);

}

void sendStatus() {
  char topic_temp[80] = "";
  snprintf (topic_temp, 80, "%s/status", mqtt_topic);
  client.publish(topic_temp, "ONLINE");
}

void reconnect() {
  taskScheduler.attach(0.5, blinkLed); //parpadear led
  // Loop until we're reconnected
  //while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "IOT-";
    clientId += String(esp_device_id);
    // Attempt to connect
    //Serial.print(" u " + String(server_user) + " / pass " + String(server_pass) + "  );
    if (client.connect(clientId.c_str(), server_user, server_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "Connection");
      // ... and resubscribe
      client.subscribe("inTopic");
      sendToServerInit();
      client.subscribe(mqtt_topic);
      char topic_temp[80] = "";
      snprintf (topic_temp, 80, "%s/%s", mqtt_topic, "setRGB");
      client.subscribe(topic_temp);
      snprintf (topic_temp, 80, "%s/%s", mqtt_topic, "set");
      client.subscribe(topic_temp);
      sendStatus();
      sendMotta(false);
      sendMotta(light_3K, light_4K);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }

    taskScheduler.detach();
    digitalWrite(LED, 0);
  //}
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, mqtt_topic) == 0) {
    callbackNormal(topic, payload, length);
  } else {
    callbackMotta(topic, payload, length);
  }
}

void callbackMotta(char* topic, byte* payload, unsigned int length) {
  char* value = (char*)malloc(length);
  // Copy the payload to the new buffer
  memcpy(value,payload,length);

  if (strstr (topic, "/setRGB")) {
    int l3k, l4k;
    sscanf(value, "%d,%d", &l3k, &l4k);
    manager(l3k, l4k);
  } else if (strstr (topic, "/set")) {
    bool status = false;
    if (strstr (value, "ON")) {
      status = true;
    }
    manager(status);
  } 
  
  bool status = false;
  if (strstr (value, "ON")) {
    status = true;
  }

}

// Callback function
void callbackNormal(char* topic, byte* payload, unsigned int length) {
  // In order to republish this payload, a copy must be made
  // as the orignal payload buffer will be overwritten whilst
  // constructing the PUBLISH packet.
    /*
     * deberia recibir un mensaje como el siguiente:
     * {"clientId":"HOME","relay-1":"true"}
     * 
     */
  Serial.println("Se recibe un mensaje.");
  // Allocate the correct amount of memory for the payload copy
  /*
  byte* p = (byte*)malloc(length);
  // Copy the payload to the new buffer
  memcpy(p,payload,length);
  Serial.println(p);
  client.publish("outTopic", p, length);
  // Free the memory
  free(p);
  */
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  char json[length];
  for (int i=0;i<length;i++) {
      char receivedChar = (char)payload[i]; 
      json[i] = (char)payload[i];
      Serial.print(receivedChar);
  }
  Serial.println();
  const size_t capacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(2) + 60;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success()) {
      Serial.println(F("Parsing failed!"));
  }

  bool token_ok = false;
  for (auto kv : root) {
    if (strcmp(kv.key, "token") == 0) {
      if (strcmp(kv.value, token) == 0) {
        //ok 
        token_ok = true;
      } else {
        char msg[200];
        snprintf (msg, 200, "{ \"clientId\" : \"ESP-%s\", \"error\" : \"Wrong token error.\" }", esp_device_id);
        client.publish("error", msg);
        return;
      }
    }
    
  }

  if (!token_ok) {
      char msg[200];
      snprintf (msg, 200, "{ \"clientId\" : \"ESP-%s\", \"error\" : \"No token error.\" }", esp_device_id);
      client.publish("error", msg);
      return;  
    }
  
  char stat[7] = "false";
  if (root["status"]) {
    enviarEstado();
  }

  int l3k = 0;
  int l4k = 0;

  for (auto kv : root) {
    Serial.print(kv.key);
    Serial.print(" -> ");
    Serial.println(kv.value.as<char*>());
    if ((strcmp(kv.key, "3k") == 0)||(strcmp(kv.key, "3K") == 0)) {
      Serial.println("3000 k light");
      l3k = kv.value;
    }
    if ((strcmp(kv.key, "4k") == 0)||(strcmp(kv.key, "4K") == 0)) {
      Serial.println("4000 k light");
      l4k = kv.value;
    }

    manager(l3k, l4k);
    
  }
  
}
