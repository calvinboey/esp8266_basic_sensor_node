
const char* ssid = "ssid_here";
const char* password = "wifi_pass";
const char* mqtt_server = "test.mosquitto.org"; //address of MQTT server
const int mqtt_port = 1883; //port of MQTT server

const char* device_id = "topic/subtopic/sensornode"; //topic prefix
char mqtt_client_id [30];
const long reconnectTimeout = 3000; //in millisecond
const long sensorColTimeout = 10000; //in millisecond

char message [25];
char topic [50];

const int OUT_LED = 2;       //Built-in LED
const int IN_PIR = 13;       //PIR Sensor (Digital)
const int INOUT_DHT = 12;    //DHT11 temp & humidity
