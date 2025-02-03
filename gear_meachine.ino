#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "HUAWEI_C47D";
const char* password = "12345678";

// MQTT broker details
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

// MQTT topics
const char* diversityCountTopic = "diversity/count"; // Publish diversity counts here
const char* toolChangeTopic = "tool/change";        // Subscribe to tool change requests
const char* diversitySelectTopic = "diversity/select"; // Subscribe to diversity selection

// GPIO pin for proximity sensor
const int proximityPin = 4;

// Variables for counting and diversity
int diversityCount[4] = {0, 0, 0, 0}; // Counts for each diversity
int currentDiversity = 0; // Currently selected diversity
bool proximityTriggered = false;
bool toolChangeRequested = false;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Handle tool change request
  if (String(topic) == toolChangeTopic) {
    if (message == "reset") {
      toolChangeRequested = true;
      Serial.println("Tool change requested: Count reset.");
    }
  }

  // Handle diversity selection
  if (String(topic) == diversitySelectTopic) {
    currentDiversity = message.toInt() - 1; // Assuming diversity is 1-4
    Serial.print("Diversity selected: ");
    Serial.println(currentDiversity + 1);
  }
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(toolChangeTopic);
      client.subscribe(diversitySelectTopic);
    } else {
      Serial.println(" Failed. Retrying in 5s");
      delay(5000);
    }
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    setup_wifi();  
  }
  connectMQTT();
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  connectMQTT();

  pinMode(proximityPin, INPUT);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Check if proximity sensor is triggered
  if (digitalRead(proximityPin) == HIGH && !proximityTriggered) {
    proximityTriggered = true;
    diversityCount[currentDiversity]++;
    Serial.print("Diversity ");
    Serial.print(currentDiversity + 1);
    Serial.print(" count: ");
    Serial.println(diversityCount[currentDiversity]);

    // Publish the count to MQTT
    char countMessage[10];
    snprintf(countMessage, sizeof(countMessage), "%d", diversityCount[currentDiversity]);
    client.publish(diversityCountTopic, countMessage);
  } else if (digitalRead(proximityPin) == LOW) {
    proximityTriggered = false;
  }

  // Handle tool change request
  if (toolChangeRequested) {
    diversityCount[currentDiversity] = 0; // Reset the count for the current diversity
    toolChangeRequested = false;
    Serial.print("Diversity ");
    Serial.print(currentDiversity + 1);
    Serial.println(" count reset.");

    // Publish the reset count to MQTT
    client.publish(diversityCountTopic, "0");
  }
}