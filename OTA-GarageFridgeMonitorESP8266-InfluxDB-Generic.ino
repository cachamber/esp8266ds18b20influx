/*
Chambeers temperature monitor


Supports OTA Uploads
Supports up to 3 DS18B20s on ONE_WRIE_BUS
Sends temp measurements once a minute to influxDB

 */
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <InfluxDb.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// OneWire sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 13
#define TEMPERATURE_PRECISION 12

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// InfluxDB setup
const char influxDBHost[] = "XXX.XXX.XXX.XXX";
const char influxDBName = "XXXXXXXX";
Influxdb influx(influxDBHost);

const unsigned long postRate = 60000;
unsigned long lastPost = 0;

// Wireless Information
const char *ssid = "";
const char *password = "";

// This can be extended past 3
// TODO update to dynamically allocate based on number of devices found
// TODO now that influxDB and preparing the metrics, don't have to statically define
DeviceAddress devices[3];
double measurements[] = {0.0, 0.0, 0.0};
String names[] = {"NC", "NC", "NC"};

// Number of devices
int num_devices = 0;

// Convert double to string
char *dtostrf(double val, signed char width, unsigned char prec, char *s);

String convertDeviceAddress(DeviceAddress da)
{   
    String tempString = "";
     for (uint8_t i = 0; i < 8; i++)
     {
         // zero pad the address if necessary
         if (da[i] < 16) tempString += "0";
         tempString += String(da[i], HEX);
      }
  return (tempString);
}

void GetTemps()
{

  sensors.requestTemperatures(); // Send the command to get temperatures

  for (int i=0; i<num_devices; i++)
  {
    measurements[i] = sensors.getTempFByIndex(i);
  }
 
}

int postToinfluxDB()
{ 
  // LED turns on when we enter, it'll go off when we 
  // successfully post.
  digitalWrite(LED_BUILTIN, LOW);

  // last two bytes of the MAC (HEX'd) to name
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String postedID = "ccESP8266-" + macID;
  
  // Get our current temps
  GetTemps();
  
    
  // Add the four field/value pairs defined by our stream:
  // TODO update to use device count from bus
  for (int i=0; i < 3; i++)
  {   InfluxData row("temperatures");
      String id = "id" + (String)i;
      String temps = "temp" + (String)i;
      
      row.addTag("device", postedID);    
      row.addTag("sensor", names[i]);
      row.addValue("temp", measurements[i]);
      
      // Print our stuff out to serial
      Serial.print(id);
      Serial.print(": ");
      Serial.print(names[i]);
      Serial.print(" :");
      Serial.print(" Temp: ");
      Serial.println(measurements[i]);
      influx.prepare(row);
  }
  influx.write();
  
  // Before we exit, turn the LED off.
  digitalWrite(LED_BUILTIN, HIGH);
  return 1; // Return success
}

// Start here
void loop ( void ) {
  ArduinoOTA.handle();
  if ((lastPost + postRate <= millis()) || lastPost == 0)
  {
    Serial.println("Posting to influx");
    if (postToinfluxDB())
    {
      lastPost = millis();
      Serial.println("Post Suceeded");
    }
    else // If the post failed
    {
      delay(500); // Short delay, then try again
      Serial.println("Post failed, will try again.");
    }
  } 
  
  delay(50);
}

void setup ( void ) {
  
  pinMode ( LED_BUILTIN, OUTPUT );
 
  digitalWrite ( LED_BUILTIN, LOW );
  Serial.begin ( 115200 );

  delay(1000);
  // Start Network
  WiFi.begin ( ssid, password );
  Serial.println ( "" );

  // Wait for connection
  while ( WiFi.status() != WL_CONNECTED ) {
    digitalWrite(LED_BUILTIN, LOW);
    delay ( 500 );
    Serial.print ( "." );
    digitalWrite(LED_BUILTIN, HIGH);
  }
  Serial.println ( "" );
  Serial.print ( "Connected to " );
  Serial.println ( ssid );
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.localIP() );
  
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  if ( MDNS.begin ( "esp8266" ) ) {
    Serial.println ( "MDNS responder started" );
  }
  // OTA Updates here
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  // END OTA Updates
  
  // Start up the OneWire library
  sensors.begin(); 
  
  // locate devices on the bus
  Serial.print("\n\nLocating devices...");
  Serial.print("Found ");
  num_devices = sensors.getDeviceCount();
  Serial.print(num_devices, DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: ");
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  Serial.print("Setting precision to: ");
  Serial.println(TEMPERATURE_PRECISION, DEC);
  
  for (int i=0; i<num_devices; i++)
  {
      Serial.print("Getting device ");
      Serial.print(i, DEC);
      Serial.println(" information");
      if (!sensors.getAddress(devices[i], i)) 
      {  
         Serial.print("ERROR: Unable to find address for Device ");
         Serial.println(i,DEC);
      }
      names[i] = convertDeviceAddress(devices[i]);
      Serial.print("Device ");
      Serial.print(i,DEC);
      Serial.print(": ");
      Serial.println(names[i]);
  }
  
  // Setup connection to influxDB
  influx.setDb(influxDBName);
}
