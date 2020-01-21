// Thanks to https://github.com/sabas1080/OrdersNotificationTindie for the inspiration and the main outline

#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // version 5 is required, some refactor needed for version 6

const char* ssid     = "*********";     // your network SSID (name of wifi network)
const char* password = "*********"; // your network password
int MINUTES_INBETWEEN_CHECKS = 15;


String username = "************";
String apiKey = "***********";
const char*  server = "www.tindie.com";  // Server URL


WiFiClientSecure client;

char json_string[512];
StaticJsonBuffer<512> jsonBuffer;
int length = 0;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);
  Serial.println();
}

void loop() {
  Serial.println("Time to check for new orders!\n");
  checkForNewOrders();

  disconnectWifi(); // reconnect wifi on each attempt, easier than handling timeouts/leases/etc.

  Serial.print("Waiting ");
  Serial.print(MINUTES_INBETWEEN_CHECKS);
  Serial.println(" minutes, then checking again.");
  delay(1000 * 60 * MINUTES_INBETWEEN_CHECKS); // wait MINUTES_INBETWEEN_CHECKS minutes before trying again
}

void checkForNewOrders() {
  Serial.print("Attempting to connect to Wifi SSID: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.println("Connected to Wifi!");
  Serial.println("Starting connection to server...");
  if (!client.connect(server, 443))
    Serial.println("Connection failed!");
  else {
    Serial.println("Connected to server!");
    Serial.println();
    // Make a HTTP request:
    client.println("GET /api/v1/order/?format=json&shipped=false&username=" + username + "&api_key=" + apiKey + " HTTP/1.1");
    client.println("Host: www.tindie.com");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: close");
    client.println();

    handleResponse();
  }
}

void handleResponse() {
  // reset variables
  length = 0;
  json_string[0] = 0;
//  for (int i=0; i < 512; i++) {
//    json_string[i] = "";
//  }

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
//      Serial.println("headers received");
      break;
    }
  }
  Serial.print("API Response: ");
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
    json_string[length] = c;
    length++;
    Serial.write(c);
  }
  Serial.println();

  JsonObject& root = jsonBuffer.parseObject(json_string);
  if (!root.success()) {
    Serial.println(F("parseObject() failed"));
    return;
  }
  
  int orders = root["meta"]["total_count"];
  Serial.print("\nPending Orders: ");
  Serial.println(orders);
  Serial.println();

  if (orders > 0){
    Serial.println("************************");
    Serial.println("**You have new orders!**");
    Serial.println("************************");
  }

  Serial.println("\nDisconnecting from server.");
  client.stop();
}

void disconnectWifi() {
  WiFi.disconnect();
  Serial.println("Wifi disconnected.");
}
