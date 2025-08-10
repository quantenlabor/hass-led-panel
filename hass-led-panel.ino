// === Include Required Libraries ===
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266Ping.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

// === Constants ===
#define NUM_LEDS 10
#define LED_PIN D2
#define AP_SSID "hass-led-panel"
#define LOOPDELAY 5000
//LOOPDELAY is in ms; typically 5000 to 30000 (5 to 30 seconds). Higher values reduce responsiveness and server load

#define E_OK          0    // Success
#define E_ERROR      -1    // Error
#define E_TIMEOUT    -2    // Timeout
#define E_OTHER      -3    // Other/unexpected
#define E_BLACK      -4    // this LED stays black (for null:// URLs)

#define STATE_RED    -11   // HASS entity state red
#define STATE_YELLOW -12   // HASS entity state yellow
#define STATE_GREEN   10   // HASS entity state green
#define STATE_BLACK  -14   // this LED stays black (for null:// URLs)


// === Global Objects ===
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
AsyncWebServer server(80);

// === Global Variables ===
String ssid, password;
bool wifiConnected = false;

// === Configuration Structure ===
struct Config {
  String server[NUM_LEDS - 1];
  int timeout = 20;
  int brightness = 20;
  String hass_server_url;
  String hass_llat;
} config;


// === LED Utility Functions ===
void blue(int i) {
  strip.setPixelColor(i, strip.Color(0, 0, 255));
  strip.show();
}

void setAllLEDsColor(uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, color);
  strip.show();
}

// === WiFi Setup Functions ===
void startAPMode() {
  WiFi.softAP(AP_SSID);
  Serial.println("Started AP mode.");
}



// === Handler: Serve WiFi Config Page ===

void handleWiFiConfigPage(AsyncWebServerRequest *request) {
  const String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>HomeAssistant LED Panel - WiFi Configuration</title>
    <style>
      body { font-family: Arial, sans-serif; margin: 40px; }
      label { display: block; margin-top: 10px; }
      input[type="text"], input[type="password"] {
        width: 300px; padding: 8px; margin-top: 5px;
      }
      input[type="submit"] {
        margin-top: 20px; padding: 10px 20px;
      }
    </style>
  </head>
  <body>
    <h2>Enter WiFi Credentials</h2>
    <form action="/save" method="POST">
      <label for="ssid">WiFi SSID:</label>
      <input type="text" id="ssid" name="ssid" placeholder="Enter SSID" maxlength=""100" required>

      <label for="password">WiFi Password:</label>
      <input type="text" id="password" name="password" placeholder="Enter Password" maxlength=""100" required>

      <input type="submit" value="Save">
      <p><br/>After saving, connect to <a href="http://hass-led-panel/">http://hass-led-panel/</a> or look up the IP address in your router.</p>
    </form>
  </body>
  </html>
  )rawliteral";
  request->send(200, "text/html", html);
}


bool loadWiFiCredentials() {
  if (!LittleFS.exists("/wifi.json")) return false;
  File file = LittleFS.open("/wifi.json", "r");
  DynamicJsonDocument doc(512);
  deserializeJson(doc, file);
  ssid = doc["ssid"].as<String>();
  password = doc["password"].as<String>();
  file.close();
  return true;
}

void saveWiFiCredentials() {
  DynamicJsonDocument doc(512);
  doc["ssid"] = ssid;
  doc["password"] = password;
  File file = LittleFS.open("/wifi.json", "w");
  serializeJson(doc, file);
  file.close();
}

// === Handler: Save WiFi Credentials ===
void handleWiFiCredentialsSave(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
    ssid = request->getParam("ssid", true)->value();
    password = request->getParam("password", true)->value();
    saveWiFiCredentials();
    ESP.restart();
  } else {
    request->send(400, "text/plain", "Missing parameters");
  }
}

// === WiFi Config Page Setup ===
void showWiFiConfigPage() {
  server.on("/", HTTP_GET, handleWiFiConfigPage);
  server.on("/save", HTTP_POST, handleWiFiCredentialsSave);
  server.begin();
}


void loadConfig() {
  Serial.println("Reading config...");
  if (!LittleFS.exists("/config.json")) {
    Serial.println("LittleFS file /config.json does no exist!");
    return;
  }
  File file = LittleFS.open("/config.json", "r");
  DynamicJsonDocument doc(1024 + NUM_LEDS * 128);
  deserializeJson(doc, file);
  for (int i = 0; i < NUM_LEDS - 1; i++) {
    config.server[i] = doc["server"][i].as<String>();
    Serial.printf("Server[%d[]=%s\n", i, config.server[i].c_str());
  }
  config.timeout = doc["timeout"]; Serial.printf("Timeout:%d\n",config.timeout);
  config.brightness = doc["brightness"]; Serial.printf("Brightness: %d\n",config.brightness);
  config.hass_server_url = doc["hass_url"].as<String>(); Serial.printf("HASS Server: %s\n",config.hass_server_url.c_str());
  config.hass_llat = doc["hass_llat"].as<String>(); Serial.printf("HASS LLAT: %s...\n", config.hass_llat.substring(0,10).c_str()); // show only the first few characters for (minimal) security
  file.close();
  Serial.println("Config read.");
}

void saveConfig() {
  Serial.println("Saving config...");
  DynamicJsonDocument doc(1024+128*NUM_LEDS);
  JsonArray arr = doc.createNestedArray("server");
  for (int i = 0; i < NUM_LEDS - 1; i++) arr.add(config.server[i]);
  doc["timeout"] = config.timeout;
  doc["brightness"] = config.brightness;
  doc["hass_url"] = config.hass_server_url;
  doc["hass_llat"] = config.hass_llat;
  File file = LittleFS.open("/config.json", "w");
  serializeJsonPretty(doc, Serial); // pretty-print the JSON document to the serial port
  serializeJson(doc, file);
  file.close();
}


// === Handler: Serve Configuration Page ===
//void handleConfigPage(AsyncWebServerRequest *request) {
//  request->send(LittleFS, "/config.html", "text/html");
//}

void handleConfigPage(AsyncWebServerRequest *request) {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>WiFi Statuslight Configuration</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background-color: #f4f4f4;
          margin: 0;
          padding: 20px;
        }
        h2 {
          color: #333;
        }
        form {
          background: #fff;
          padding: 20px;
          border-radius: 8px;
          max-width: 500px;
          margin: auto;
          box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        label {
          display: block;
          margin-top: 15px;
          font-weight: bold;
        }
        input[type="text"],
        input[type="number"] {
          width: 100%;
          padding: 8px;
          margin-top: 5px;
          border: 1px solid #ccc;
          border-radius: 4px;
        }
        input[type="submit"] {
          margin-top: 20px;
          background-color: #4CAF50;
          color: white;
          padding: 10px 15px;
          border: none;
          border-radius: 4px;
          cursor: pointer;
        }
        input[type="submit"]:hover {
          background-color: #45a049;
        }
      </style>
    </head>
    <body>
      <h2>Device Configuration</h2>
      <form method='POST' action='/save_config'>
  )rawliteral";

  for (int i = 0; i < NUM_LEDS - 1; i++) {
    html += "<label for='server" + String(i) + "'>Server " + String(i) + ":</label>";
    html += "<input type='text' name='server" + String(i) + "' value='" + config.server[i] + "'>";
  }

  html += "<label for='timeout'>Timeout (1-60):</label>";
  html += "<input type='number' name='timeout' min='1' max='60' value='" + String(config.timeout) + "'>";

  html += "<label for='brightness'>Brightness (1-255):</label>";
  html += "<input type='number' name='brightness' min='1' max='255' value='" + String(config.brightness) + "'>";

  html += "<label for='hass_url'>HASS URL:</label>";
  html += "<input type='text' name='hass_url' value='" + config.hass_server_url + "'>";

  html += "<label for='hass_llat'>HASS LLAT:</label>";
  html += "<input type='text' name='hass_llat' value='" + config.hass_llat + "'>";

  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<h2>URL syntax</h2>";
  html += "http://servername.example.org/ or https://servername.example.org/index.html <br/>";
  html += "tcp://servername.example.org:port<br/>";
  html += "udp://servername.example.org:port<br/>";
  html += "ping://servername.example.org<br/>";
  html += "null://does_not_matter (LED remains dark)<br/>";
  html += "hass://entity_id:RED:YELLOW:GREEN - Home Assistant entity ID<br/";
  html += "This requires Home Assistant URL and access token to be set!<br/>";
  html += "RED, YELLOW and GREEN represent the entity states that make the LED light up in red, yellow, or green. All other states result in a dark LED. Case inseintitive.<br/>";
  html += "The HASS request is built like this: HASS URL + /api/states/entity_id/ + entity ID given in the hass:// line.<br/></p>";
  html += "</body></html>";

  request->send(200, "text/html", html);
}

// === Handler: Save Configuration Data ===
void handleConfigSave(AsyncWebServerRequest *request) {
  for (int i = 0; i < NUM_LEDS - 1; i++) {
    String paramName = "server" + String(i);
    if (request->hasParam(paramName, true)) {
      config.server[i] = request->getParam(paramName, true)->value();
    }
  }

  if (request->hasParam("timeout", true)) {
    config.timeout = constrain(request->getParam("timeout", true)->value().toInt(), 1, 60);
  }

  if (request->hasParam("brightness", true)) {
    config.brightness = constrain(request->getParam("brightness", true)->value().toInt(), 1, 255);
  }

  if (request->hasParam("hass_url", true)) {
    config.hass_server_url = request->getParam("hass_url", true)->value();
  }

  if (request->hasParam("hass_llat", true)) {
    config.hass_llat = request->getParam("hass_llat", true)->value();
  }

  saveConfig();
  delay(1000);
  ESP.restart();
}


// === Configuration Page Setup ===
void showConfigPage() {
  server.on("/", HTTP_GET, handleConfigPage);
  server.on("/save_config", HTTP_POST, handleConfigSave);
  server.begin();
}

// === Server Check Function ===

// helpers

int check_http(String url) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(config.timeout * 1000);
  http.begin(client, url);
  int code = http.GET();
  http.end();
  Serial.printf(" %d ", code);
  if (code < 0) return E_ERROR;
  return code < 400 ? E_OK : E_ERROR;
}

int check_https(String url){
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.setTimeout(config.timeout * 1000);
    https.begin(client, url);
    int code = https.GET();
    https.end();
    Serial.printf(" %d ", code);
    if (code < 0) return E_ERROR;
    return code < 400 ? E_OK : E_ERROR;
}

int check_tcp(String url) {
    String host = url.substring(6);
    int colon = host.indexOf(':');
    String hostname = host.substring(0, colon);
    int port = host.substring(colon + 1).toInt();
    WiFiClient client;
    return client.connect(hostname.c_str(), port) ? E_OK : E_ERROR; 
}

int check_udp(String url) {
    String host = url.substring(6);
    int colon = host.indexOf(':');
    String hostname = host.substring(0, colon);
    int port = host.substring(colon + 1).toInt();
    WiFiUDP udp;
    udp.beginPacket(hostname.c_str(), port);
    udp.write("ping");
    return udp.endPacket() ? E_OK : E_ERROR;
}

int check_ping(String url) {   
  String host = url.substring(7);
  return Ping.ping(host.c_str()) ? E_OK : E_ERROR;  
}

// helper to dissect a HASS pseudo-URL

void parseHassString(String input, String &entity_id, String &red, String &yellow, String &green) {
  // Remove the "hass://" prefix
  const String prefix = "hass://";
  if (!input.startsWith(prefix)) {
    entity_id = red = yellow = green = "";
    return; // Invalid format
  }

  input = input.substring(prefix.length()); // Strip prefix

  // Split the remaining string by ':'
  int firstColon = input.indexOf(':');
  int secondColon = input.indexOf(':', firstColon + 1);
  int thirdColon = input.indexOf(':', secondColon + 1);

  // Extract entity_id
  if (firstColon == -1) {
    entity_id = input;
    red = yellow = green = "";
    return;
  }
  entity_id = input.substring(0, firstColon);

  // Extract red
  if (secondColon == -1) {
    red = input.substring(firstColon + 1);
    yellow = green = "";
    return;
  }
  red = input.substring(firstColon + 1, secondColon);

  // Extract yellow
  if (thirdColon == -1) {
    yellow = input.substring(secondColon + 1);
    green = "";
    return;
  }
  yellow = input.substring(secondColon + 1, thirdColon);

  // Extract green
  green = input.substring(thirdColon + 1);
}

int check_hass(String url) {
  String path = url.substring(7);
  if (!path.startsWith("/")) path = "/" + path;

  String entity_id, red, yellow, green;
  parseHassString(url, entity_id, red, yellow, green);
  Serial.println("Entity ID: " + entity_id);
  Serial.println("Red: " + red);
  Serial.println("Yellow: " + yellow);
  Serial.println("Green: " + green);

  String fullUrl = config.hass_server_url + "/api/states/" + entity_id;
  Serial.println("Full HASS URL="+ fullUrl);

  // set up connection to HASS API, using the long-lived authentication token to authorize access
  HTTPClient http;

  if (String(fullUrl).startsWith("https")) {
    WiFiClientSecure secureClient;
    secureClient.setInsecure();  // Skip certificate validation
    http.begin(secureClient, fullUrl);
  } else {
    WiFiClient client;
    http.begin(client, fullUrl);
  }

  http.addHeader("Authorization", "Bearer " + config.hass_llat);
  http.addHeader("Content-Type", "application/json");

  // get API response 
  int code = http.GET();
  Serial.printf("HTTP response %d ", code);
  if (code != 200) return E_BLACK;
  String payload = http.getString(); // get response in JSON from HASS
  Serial.print(payload);
  http.end();
  DynamicJsonDocument doc(1024); // decode JSON
  deserializeJson(doc, payload);
  String state = doc["state"];
  Serial.printf(" State=%s ", state.c_str());
  if (state.equalsIgnoreCase(green)) return STATE_GREEN;
  if (state.equalsIgnoreCase(yellow)) return STATE_YELLOW;
  if (state.equalsIgnoreCase(red)) return STATE_RED;
  return STATE_BLACK; 
}

int checkServer(String url) {
  if (url.startsWith("http://")) {
    return check_http(url);
  } else if (url.startsWith("https://")) {
    return check_https(url);
  } else if (url.startsWith("tcp://")) {
    return check_tcp(url);
  } else if (url.startsWith("udp://")) {
    return check_udp(url);
  } else if (url.startsWith("ping://")) {
    return check_ping(url);
  } else if (url.startsWith("hass://")) {
        return check_hass(url);
  } else if ((url == "") || url.startsWith("null://")) {
    return E_BLACK;
  }
  return E_TIMEOUT;
}

// === Setup ===
void setup() {
  
  Serial.begin(115200);
  Serial.println("Starting Setup()");
  strip.begin();
  
  // turn LEDs blue one by one; helps to determine the order of servers
  if(config.brightness <10) config.brightness=10;
  strip.setBrightness(config.brightness);
  for(int i=0; i<NUM_LEDS; i++) {
    blue(i);
    delay(500);
  }
  
  if (!LittleFS.begin()) {
    Serial.println("LittleFS failed to mount.");
    return;
  }

  if (!loadWiFiCredentials()) {
    Serial.println("No WiFi credentials. Connect your WiFi client to built-in access point and provide some.");
    startAPMode();
    showWiFiConfigPage();
    return;
  }

  Serial.printf("Connecting to WiFi SSID=%s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected) {
    Serial.println("WiFi connected.");
    setAllLEDsColor(strip.Color(0, 0, 0)); // Off
    loadConfig();
    showConfigPage();
  } else {
    Serial.println("WiFi failed.");
    setAllLEDsColor(strip.Color(255, 0, 0)); // Red
  }
}

// === Main Loop ===
void loop() {
  Serial.println("Loop()");
  // strip.clear();
  // dispaly WiFi status on LED #0
  strip.setBrightness(config.brightness);
  strip.setPixelColor(0, wifiConnected ? strip.Color(0, 255, 0) : strip.Color(255, 0, 0));
  strip.show();
  if (!wifiConnected) return;

  for (int i = 0; i < NUM_LEDS - 1; i++) {
    Serial.printf("Testing server %d: %s...", i, config.server[i].c_str());
    int result = checkServer(config.server[i]);
    uint32_t color;
    switch (result) {
      case STATE_GREEN:
      case E_OK:      color = strip.Color(0, 255, 0);     Serial.println("OK");break;      // Green
      case STATE_RED:
      case E_ERROR:   color = strip.Color(255, 0, 0);     Serial.println("ERROR");break;   // Red
      case STATE_YELLOW:
      case E_TIMEOUT: color = strip.Color(255, 165, 0);   Serial.println("TIMEOUT");break; // Orange/yellow
      case STATE_BLACK:
      case E_BLACK:   color = strip.Color(0, 0, 0);       Serial.println("BLACK");break;   // Black (dark)
      default:        color = strip.Color(255, 255, 255); Serial.println("WHITE");break;   // White
    }
    strip.setPixelColor(i + 1, color);
    strip.show();
  }
  
  delay(LOOPDELAY);
}
