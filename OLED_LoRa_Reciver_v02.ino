/*
 * ____        __                  __
 * |     /\   |  \  |       /\    /  \
 * |--  /--\  |__/  |      /--\   |
 * |   /    \ |   \ |     /    \  |  
 * |  /      \|___/ |___ /      \ |__/
 * 
 * Essai des modules TTGo LoRa32 v1 avec Affichage Oled
 * Module de reception des données envoyées par les sondes de temperature
 * Licence de droits: Attribution 4.0 International (CC BY 4.0)
 * 
 * Créé le 07/06/2023 Paddy
 * v0.1b1: test de reception LoRa du fablac jusqu'au port d'Anthy OK
 * v0.1b2: (20/06/2023) reception de 2 valeurs depuis un meme emetteur
 * v0.1b3: Connexion au wifi du fab pour obtenir IP
 *         Ajout d'un serveur web asynchrome qui donne les 2 températures
 * v0.1b4 (07/05/2024): 
 *      - test de reception de 2 emetteurs
 * v0.2   (14/05/2024):
 *      - modification pour conserver les 2 derniers paquets recus
 *      - modification affichage pour avoir toutes les infos
 *      - ajout d'une police dans Roboto_Condensed_14 dans la bibliotheque ESP8266_and_ESP32_OLED_driver_for_SSD1306_displays/src/OLEDDisplayFonts.h
 *      - police faite avec http://oleddisplay.squix.ch/
 *      
 */

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include "SSD1306.h"
// #include <images.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include "ESPAsyncWebServer.h"

#include <SPIFFS.h>

// Libraries to get time from NTP Server
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "SSDI";          //Your SSID
const char* password = "PASSWORD";  //Your Password

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    868E6
//433E6 for Asia
//866E6 for Europe
//915E6 for North America

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String day;
String hour;
String timestamp;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const int led = 13;

// Initialize variables to get and save LoRa data
String loRaMessage;
String temp1;
String temp2;
String lieu;

SSD1306 display(0x3c, 4, 15);
String rssi = "RSSI --";
String packSize = "--";
String packet ;
String Trecu = "";
String Told = "";
int counter = 0; //nb de paquet reçu
String LoRaData = "" ;

void loraData() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(Roboto_Condensed_14);
  display.drawStringMaxWidth(0 , 0 , 128, Trecu);
  display.setFont(ArialMT_Plain_10);
  display.drawStringMaxWidth(0 , 15 , 128, Told);
  display.drawString(0 , 30 , "Paquet " + String(counter));
  display.drawString(0 , 45 , "Size " + packSize + " bytes / " + rssi);
  //display.drawString(0, 45, rssi);
  display.display();
}

void cbk(int packetSize) {
  counter++;
  packet = "";
  packSize = String(packetSize, DEC);
  for (int i = 0; i < packetSize; i++) {
    packet += (char) LoRa.read();
  }
  Trecu = packet;
  rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
  loraData();
}

// Read LoRa packet and get the sensor readings
void getLoRaData() {
  Serial.print("Lora packet received: ");
  // Read packet
  while (LoRa.available()) {
    String LoRaData = LoRa.readString();
    // LoRaData format: temp1/temp2
    // String example: 23.50/21.75
    // Ajout adresse sender
    // ex: 23.50/21.75?anthy
    Serial.print(LoRaData);
    Told = Trecu;
    Trecu=LoRaData;
    counter++;
    rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
    loraData();

    // cbk(LoRaData.length());
    // Get readingID, temperature and soil moisture
    int pos1 = LoRaData.indexOf('/');
    int pos2 = LoRaData.indexOf('?');
    temp1 = LoRaData.substring(0, pos1);
    temp2 = LoRaData.substring(pos1 + 1, pos2); 
    lieu= LoRaData.substring(pos2 + 1, LoRaData.length());
  }
  // Get RSSI
  rssi = LoRa.packetRssi();
  Serial.print(" with RSSI ");
  Serial.println(rssi);
}

// Function to get date and time from NTPClient
void getTimeStamp() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  // formattedDate = timeClient.getFormattedDate();
  formattedDate="2018-05-28T16:00:13Z";
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  day = formattedDate.substring(0, splitT);
  Serial.println(day);
  // Extract time
  hour = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(hour);
  timestamp = day + " " + hour;
}


/*Fonctions pour le Serveur Web
  void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from FabLac ! Temperature is "+Trecu);
  digitalWrite(led, 0);
  }

  void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
  }
*/
// Replaces placeholder with DHT values
String processor(const String& var) {
  //Serial.println(var);
  if (var == "TEMPERATURE1") {
    return temp1;
  }
  else if (var == "TEMPERATURE2") {
    return temp2;
  }
  else if (var == "TIMESTAMP") {
    return timestamp;
  }
  else if (var == "RRSI") {
    return String(rssi);
  }
  return String();
}


void setup() {
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50);
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high、

  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.println("LoRa Receiver Callback");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DI0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  // LoRa.onReceive(cbk);
  LoRa.receive();
  Serial.println("init ok");
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  packSize = "??°";
  display.drawString(0 , 15 , "Il fait " + packSize + " Chaud!?");
  display.drawStringMaxWidth(0 , 26 , 128, packet);
  display.drawString(0, 0, rssi);
  display.display();

  //Setup Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  /*
   * Inutile pour la V1.6 du TTGo LoRa qui ne dispose pas de TFCard
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    delay(1000);
      if (!SPIFFS.begin()) {
        Serial.println("An Other error has occurred while mounting SPIFFS");
        delay(10000);
        if (!SPIFFS.begin()) {
          Serial.println("3 errors has occurred while mounting SPIFFS !! Too much ! Forget it ");
        }
      }
  }
  */
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String("Hello from FabLac @ Anthy\n\nTemperatures are (at 1m/2m): "+temp1+"/"+temp2+"\nData from "+lieu).c_str());
  });
  server.on("/temperature1", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", temp1.c_str());
  });
  server.on("/temperature2", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", temp2.c_str());
  });
  server.on("/timestamp", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", timestamp.c_str());
  });
  server.on("/where", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", lieu.c_str());
  });
  server.on("/rssi", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", String(rssi).c_str());
  });
  server.on("/winter", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/winter.jpg", "image/jpg");
  });
  // Start server
  server.begin();
  Serial.println("HTTP server started");

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200);



  delay(1500);
}

void loop() {
  int packetSize = LoRa.parsePacket();
  delay(2);
  if (packetSize) {
    packSize = String(packetSize, DEC);
    getLoRaData();
    // cbk(packetSize);
    // getTimeStamp();
  }
  //delay(2);
  //server.handleClient();
}
