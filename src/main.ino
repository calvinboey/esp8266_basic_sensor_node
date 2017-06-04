#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <devconf.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <DHT.h>

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(INOUT_DHT, DHT11, 20);
long now; //use by clock
long just_now; //use by clock
long lastReconnectAttempt = 0;
long lastSensorCol = 0;
int reconnectCounter = 0;
char sub_topic[15];
int debug_count = 0;
int pir_in;
int pir_state;
int pir_old_in;

//Adafruit's TSL2561 Digital Lux Sensor Library
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

void configureSensor(void) {
  tsl.enableAutoRange(true);
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
}

boolean reconnect() {
  reconnectCounter++;
  if(reconnectCounter > 3){
    Serial.println("Connection Failed! Rebooting...");
    delay(300);
    ESP.restart();
  }
  if (client.connect(mqtt_client_id)) {
    Serial.println("MQTT connected");
    //publish the ip address on MQTT
    sprintf (topic, "%s/%s", device_id, "general/ipaddr/last");
    //Uncomment. The toCharArray() has to be added in source file
    //client.publish(topic, WiFi.localIP().toCharArray());
    reconnectCounter = 0;
  }
  return client.connected();
}

void setup_mqtt(){
  client.setServer(mqtt_server, mqtt_port);
  delay(300);  //give it some time to process
  reconnect();
  delay(300);  //give it some time to process
  lastReconnectAttempt = 0;
}

//Setup is based on ESP8266 OTA template
void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(300);
    ESP.restart();
  }

  Serial.printf("%s %d", "WiFi code: ", WiFi.status());

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
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

  //setup MQTT
  randomSeed(analogRead(0));
  sprintf(mqtt_client_id, "%s%d", device_id, random(99999));
  setup_mqtt();

  //Set the PIR pin
  pinMode(IN_PIR, INPUT);
  //Initialize DHT11
  dht.begin();

  //setup TSL2561 Lux Sensor
  if(!tsl.begin()){
    Serial.println("TSL2561 not detected");
    return;
  } else {
    configureSensor();
  }

  Serial.println("finish setup code...");
}

void loop() {
  ArduinoOTA.handle();

  //run the clock
  just_now = now;
  now = millis();

  //MQTT connection check
  if (!client.connected()) {
    now = millis();
    if (now - lastReconnectAttempt > reconnectTimeout) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }

  //Get PIR and update via MQTT when there's change of state
  pir_old_in = pir_in;
  pir_in = digitalRead(IN_PIR);
  if(pir_old_in != pir_in){
    Serial.printf("motion: %d\n", pir_in);
    pir_state = pir_in;
    sprintf (topic, "%s/%s", device_id, "general/motion/last");
    sprintf (message, "%d", pir_state);
    if (client.connected()) { client.publish(topic, message); }
  }else{
    //Serial.printf("motion: %d\n", pir_in);
    pir_state = pir_in;
    sprintf (topic, "%s/%s", device_id, "general/motionneg/last");
    sprintf (message, "%d", pir_state);
    // if (client.connected()) { client.publish(topic, message); }
  }

  now = millis();
  if(now - lastSensorCol > sensorColTimeout) {
    lastSensorCol = millis();
    //Get brightness (lux)
    sensors_event_t event;
    tsl.getEvent(&event);

    if (event.light){
      int value = event.light;
      // Serial.println(value);
      sprintf (topic, "%s/%s", device_id, "general/luminosity/last");
      sprintf (message, "%d", value);
      if (client.connected()) { client.publish(topic, message); }
    } else {
      // Serial.println("0");
      sprintf (topic, "%s/%s", device_id, "general/luminosity/last");
      sprintf (message, "%d", 0);
      if (client.connected()) { client.publish(topic, message); }
    }

    //DHT11
    int h = dht.readHumidity();
    int f = dht.readTemperature(false);  //false=celcius
    if (isnan(h) || isnan(f)) {
      return;
    }
    Serial.printf("humidity: %d\n", h);
    Serial.printf("temperature: %d\n", f);
    sprintf (topic, "%s/%s", device_id, "general/humidity/last");
    sprintf (message, "%d", h);
    if (client.connected()) { client.publish(topic, message); }
    sprintf (topic, "%s/%s", device_id, "general/temperature/last");
    sprintf (message, "%d", f);
    if (client.connected()) { client.publish(topic, message); }
  } //end of timeout portion

  delay(10);
  client.loop();

}
