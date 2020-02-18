// Thanks to https://github.com/sabas1080/OrdersNotificationTindie for the inspiration and the main outline

#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // version 5 is required, some refactor needed for version 6
#include "FastLED.h"

const char* ssid     = "*********";     // your network SSID (name of wifi network)
const char* password = "*********"; // your network password
int MINUTES_INBETWEEN_CHECKS = 15;


String username = "************";
String apiKey = "***********";
const char*  server = "www.tindie.com";  // Server URL


WiFiClientSecure client;

char json_string[2048];
StaticJsonBuffer<2048> jsonBuffer;
int length = 0;

bool errorState = false;
bool newOrders = false;
long lastSuccessfulCheck = millis();

TaskHandle_t LEDTask;

#define DATA_PIN    12
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    12
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          10
#define FRAMES_PER_SECOND  120
uint8_t gHue = 0; // rotating "base color"

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(100);
  Serial.println();

  xTaskCreatePinnedToCore(
      LEDTaskcode, /* Function to implement the task */
      "LEDTask", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &LEDTask,  /* Task handle. */
      0); /* Core where the task should run */
}

void loop() {
  Serial.println("...Time to check for new orders!\n");
  bool success = sendAPIRequest(); // ask the API if we have a new order
  if (success) handleResponse();
  
  disconnect(); // reconnect to server + wifi on each attempt, easier than handling timeouts/leases/etc.

  // are we having general connection issues? if so, indicate via errorState bool
  if (millis() > lastSuccessfulCheck + (1000 * 60 * (MINUTES_INBETWEEN_CHECKS + 1))) {
    errorState = true; // we haven't been able to connect the last two attempts
  }
  if (errorState) Serial.println("\n***Experiencing connection issues, please investigate!***\n");

  Serial.print("Waiting ");
  Serial.print(MINUTES_INBETWEEN_CHECKS);
  Serial.println(" minutes, then checking again...");
  delay(1000 * 60 * MINUTES_INBETWEEN_CHECKS); // wait MINUTES_INBETWEEN_CHECKS minutes before trying again

  Serial.println("RESTARTING...");
  ESP.restart(); // seems we are having some issues with running continuously, so let's just reset entirely
}

bool sendAPIRequest() { // bool represents success, true=request sent, false=some failure
  Serial.print("Attempting to connect to Wifi SSID: ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("...");
    // wait 1 second for re-trying
    delay(1000);
  }

  Serial.println("Connected to Wifi!");
  Serial.print("Starting connection to server...");
  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
    return false;
  }
  else {
    Serial.println("Connected to server!");
    Serial.println();
    // Make a HTTP request:
    client.println("GET /api/v1/order/?format=json&shipped=false&limit=1&username=" + username + "&api_key=" + apiKey + " HTTP/1.1");
    client.println("Host: www.tindie.com");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("Connection: close");
    client.println();

    return true;
  }
}

void handleResponse() {
  // reset variables
  length = 0;
  json_string[0] = 0;
  jsonBuffer.clear();

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
//    Serial.write(c);
  }

  Serial.println(json_string);
  JsonObject& root = jsonBuffer.parseObject(json_string);
  if (!root.success()) {
    Serial.println(F("parseObject() failed"));
    return;
  }
  Serial.println();
  
  int orders = root["meta"]["total_count"];

  if (orders > 0){
    Serial.println("**********************************");
    Serial.println("**You have at least 1 new order!**");
    Serial.println("**********************************");
    
    newOrders = true;
  }
  else {
    newOrders = false;
    Serial.println("No pending orders. :(");
  }

  errorState = false;
  lastSuccessfulCheck = millis();
}

void disconnect() {
  Serial.println("\nDisconnecting from server.");
  client.stop();
  
  WiFi.disconnect();
  Serial.println("Wifi disconnected.");
}

void LEDTaskcode (void * parameter) {
  // wait a bit before starting
  delay(4000);
  
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
   
  for(;;) {
    if (errorState) { // a red dot sweeping around, with fading trails
      fadeToBlackBy(leds, NUM_LEDS, 20);
      uint16_t beatsaw = beat16( 50 /* BPM */);
      int pos = scale16( beatsaw, NUM_LEDS-1);
      leds[pos] += CRGB::Red;
    }
    else if (newOrders) { // random colored speckles that blink in and fade smoothly
      fadeToBlackBy(leds, NUM_LEDS, 10);
      int pos = random16(NUM_LEDS);
      if(random8(2) == 0) leds[pos] += CHSV(gHue + random8(64), 200, 255);
    }
    else {
      fadeToBlackBy(leds, NUM_LEDS, 10); // turn all the leds off
    }
    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/FRAMES_PER_SECOND); 
    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  }
}
