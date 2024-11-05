/*
 * ____        __                  __
 * |     /\   |  \  |       /\    /  \
 * |--  /--\  |__/  |      /--\   |
 * |   /    \ |   \ |     /    \  |  
 * |  /      \|___/ |___ /      \ |__/
 * 
 * Essai des modules TTGo LoRa32 v1 avec Affichage Oled
 * Pour mesurer la température de l'eau avec un capteur de temperature DS18B20
 * et transmettre le résultat en LoRa
 * Licence de droits: Attribution 4.0 International (CC BY 4.0)
 * 
 * Créé le 07/06/2023 Paddy
 * v0.1b1: test de transmission LoRa du fablac jusqu'au port d'Anthy OK
 * v0.1b2: ajout du capteur de temperature DS18b20
 * v0.1b3 (14/06/2023): 
 *      - Probleme au televersement à cause du capteur ds18b20 branché sur le port 2 du TTGo !!!!! Débrancher pour televerser !!!!!
 *      - changement de la resolution du capteur 11 correspond a une precision au 1/8 de ° (0.125°)
 *      - Ajout du deep sleep parametrable en secondes TIME_TO_SLEEP (default=10)
 *      - Ajout de variable enregistrée en mémoire RTC: bootCount et counter
 *      - Affichage en ArialMT_Plain_16
 * v0.1b4 (20/06/2023)
 *      - Ajout d'un second capteur affichage et emission des 2 données
 * v0.2   (08/05/2024)
 *      - Ajout d'un lieu pour différencier les emetteurs 
 *      - Format données émises temp1/temp2?lieu
 * v0.3   (14/05/2024)
 *      - TIME_TO_SLEEP passé à 1800 secondes (30 minutes) pour test batterie
 * v0.4   4/11/2024 (Jerome)
 *        Ajout SyncWord 0xFA , CodingRate4 8, SignalBandwidth 125E3, SpreadingFactor 11 , Frequency 869587500
 */
  
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>  
#include "SSD1306.h" 
#include <OneWire.h>
#include <DallasTemperature.h>

// deep sleep 
#define uS_TO_S_FACTOR 1000000  // conversion factor for micro seconds to seconds 
#define TIME_TO_SLEEP  1800        // time ESP32 will go to sleep (in seconds)   - 99 for (about) 1% duty cycle  

//identifiant de l'emetteur/sonde de temperature
String lieu = "Anthy";

#define SCK     5    // GPIO5  -- SX1278's SCK
#define MISO    19   // GPIO19 -- SX1278's MISO
#define MOSI    27   // GPIO27 -- SX1278's MOSI
#define SS      18   // GPIO18 -- SX1278's CS
#define RST     14   // GPIO14 -- SX1278's RESET
#define DI0     26   // GPIO26 -- SX1278's IRQ(Interrupt Request)
#define BAND    868E6

// Data wire connecte au port 2 du TTGo
#define ONE_WIRE_BUS 2
#define TEMPERATURE_PRECISION 11


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress Thermometer1m, Thermometer2m;

SSD1306 display(0x3c, 4, 15);
String rssi = "RSSI --";
String packSize = "--";
String packet ;

// stores the data on the RTC memory so that it will not be deleted during the deep sleep
RTC_DATA_ATTR int bootCount = 0; 
RTC_DATA_ATTR unsigned int counter = 0;   // sending packet number...

void logo(){
  display.clear();
//  display.drawXbm(0,5,logo_width,logo_height,logo_bits);
  display.display();
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void setup() {
  pinMode(16,OUTPUT);
  pinMode(2,OUTPUT);
  
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 in high
  
  Serial.begin(9600);
  while (!Serial);
  Serial.println();
  Serial.println("LoRa Sender Test + Dallas Temperature IC Control");

  sensors.begin();
    // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

    // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then
  // use those addresses and manually assign them (see above) once you know
  // the devices on your bus (and assuming they don't change).
  //
  // method 1: by index
  if (!sensors.getAddress(Thermometer1m, 0)) Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(Thermometer2m, 1)) Serial.println("Unable to find address for Device 1");

    // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(Thermometer1m);
  Serial.println();

  Serial.print("Device 1 Address: ");
  printAddress(Thermometer2m);
  Serial.println();



  // set the resolution to 9 bit per device
  sensors.setResolution(Thermometer1m, TEMPERATURE_PRECISION);
  sensors.setResolution(Thermometer2m, TEMPERATURE_PRECISION);

  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(Thermometer1m), DEC);
  Serial.println();

  Serial.print("Device 1 Resolution: ");
  Serial.print(sensors.getResolution(Thermometer2m), DEC);
  Serial.println();

  
  SPI.begin(SCK,MISO,MOSI,SS);
  LoRa.setPins(SS,RST,DI0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  LoRa.setSyncWord(0xFA);           // ranges from 0-0xFF, default 0x34, see API docs
//  LoRa.setCodingRate4(8);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setSpreadingFactor(11);
  LoRa.setFrequency(869587500); 
  // changes the spreading factor to 12 -> slower speed but better noise immunity
  //LoRa.setSpreadingFactor(12);   // ranges from 6-12, default is 7 
  //LoRa.onReceive(cbk);
//  LoRa.receive();
  Serial.println("init ok");
  //Increments boot number and prints it every reboot
  bootCount++;
  Serial.println("Boot number: " + String(bootCount));

  // display.init();
  // display.flipScreenVertically();  
  // display.setFont(ArialMT_Plain_16);
  // logo();
  delay(1500);
}

void loop() {
  // call sensors.requestTemperatures() to issue a global temperature 
  // request to all devices on the bus
  digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
  delay(100);                       // wait for a 1/10 second

  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("requete temperature...");
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  float temp1 = sensors.getTempCByIndex(0);
  float temp2 = sensors.getTempCByIndex(1);
 // Check if reading was successful
  if((temp1 && temp2) != DEVICE_DISCONNECTED_C) 
  {
    Serial.print("La temperature du capteur 1 est:");
    Serial.print(temp1);
    Serial.println("° celcius");
    Serial.print("La temperature du capteur 2 est:");
    Serial.println(temp2);    
  } 
  else
  {
    Serial.println("Error: Could not read temperature data");
  }

  /*   
   * UNCOMMENT FOR WORKING DISPLAY   
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  
  display.drawString(0, 0, "1m: ");
  display.drawString(60, 0, String(temp1));
  display.drawString(0, 20, "2m: ");
  display.drawString(60, 20, String(temp2));
  
  display.drawString(0, 40, "Nb envoi: ");
  display.drawString(90, 40, String(counter));
  display.display();
 
      
  */
  // send packet
  LoRa.beginPacket();
  //LoRa.print("T1°: ");
  LoRa.print(temp1);
  LoRa.print("/");
  LoRa.print(temp2);
  LoRa.print("?");
  LoRa.print(lieu);
  LoRa.endPacket();
  delay(1000);
  counter++;
  // deep sleep
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.println("Going to sleep now");
  Serial.flush();   // waits for the transmission of outgoing serial data to complete 
  esp_deep_sleep_start();   // enters deep sleep with the configured wakeup options

  //digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  //delay(1000);                       // wait for a second
  //digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
  //delay(1000);                       // wait for a second
}
