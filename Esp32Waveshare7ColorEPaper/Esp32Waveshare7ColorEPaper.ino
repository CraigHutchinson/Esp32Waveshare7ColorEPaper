
#include <WiFi.h>
#include "time.h"
#include <esp_sntp.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_4_01_ColourEPaper.h"

char firmwareDate[] = "07/02/2024";

/** User-provided configuration that contains SSID, WiFi wifiPassword & Octopus personal authorisation code
@code
  const char* wifiSsid = "WiFi SSID";
  const char* wifiPassword = "WiFi Password";
  const char* auth_string = "API Authorisation Code from Octopus Energy Account";
@endcode
*/
#include "secrets.h" // 

// SPI
#define DIN_PIN	14	//SPI MOSI pin, data input
#define SCLK_PIN	13	//SPI CLK pin, clock signal input
#define CS_PIN	15	//Chip selection, low active

#define DC_PIN	27	//Data/command, low for commands, high for data
#define RST_PIN	26	//Reset, low active
#define BUSY_PIN	25	//Busy status output pin (means busy)

/**********************************
Color Index
**********************************/
#define EPD_4IN01F_BLACK   0x0	/// 000
#define EPD_4IN01F_WHITE   0x1	///	001
#define EPD_4IN01F_GREEN   0x2	///	010
#define EPD_4IN01F_BLUE    0x3	///	011
#define EPD_4IN01F_RED     0x4	///	100
#define EPD_4IN01F_YELLOW  0x5	///	101
#define EPD_4IN01F_ORANGE  0x6	///	110
#define EPD_4IN01F_CLEAN   0x7	///	111   unavailable  Afterimage

#define EPD_4IN01F_WIDTH       640
#define EPD_4IN01F_HEIGHT      400


#define SCREEN_WIDTH EPD_4IN01F_WIDTH     // OLED display width, in pixels
#define SCREEN_HEIGHT EPD_4IN01F_HEIGHT     // OLED display height, in pixels

#define SCREEN_WHITE EPD_4IN01F_WHITE
#define SCREEN_BLACK EPD_4IN01F_BLACK

bool headless = false; //< Run without display
// Display Scehmatic https://files.waveshare.com/upload/b/bb/4.01inch_e-Paper_HAT_%28F%29.pdf
// Reference design https://files.waveshare.com/upload/f/f0/4.01inch-ePaper-F-Reference-Design.pdf
Adafruit_4_01_ColourEPaper display(
    SCREEN_WIDTH
  , SCREEN_HEIGHT
  , RST_PIN
  , DC_PIN
  , BUSY_PIN
  , false);

//
// Declare Functions
//
void printTime(time_t timeToPrint);  // Print time to serial monitor
void Get_Octopus_Data();             // Get Octopus Data
//void printLocalTime();
void timeavailable(struct timeval* t);  // Callback function (get's called when time adjusts via NTP)
void updateDisplay();                      // Routine Refresh of Display
void graphOLED();                       // Draw tariff graph
//
// WiFi Credentials
//
//const char* wifiSsid = "YOUR SSID HERE";  NOTE: These data now picked up from secrets.h file
//const char* wifiPassword = "YOUR PASSWORD HERE";
//
//Set API Sources
//
//const char* auth_string = "PUT YOUR OCTOPUS AUTHORISATION CODE HERE"
const char* server = "api.octopus.energy";  // Server URL
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;
const char* time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)
//
// Cert for Octopus
//
const char* octopus =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\n"
  "BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw\n"
  "MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n"
  "eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV\n"
  "UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE\n"
  "ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp\n"
  "ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n"
  "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/\n"
  "y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N\n"
  "Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo\n"
  "Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C\n"
  "zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J\n"
  "Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB\n"
  "AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O\n"
  "BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV\n"
  "rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u\n"
  "c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud\n"
  "HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG\n"
  "BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G\n"
  "VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1\n"
  "l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt\n"
  "8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ\n"
  "59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu\n"
  "VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=\n"
  "-----END CERTIFICATE-----\n";
//
//Define Variables
//
// Create arrays to store start times of each 1/2hr tariff slot and agile tariff for each
//
double tariffArray[200];
double lowestTariffVisible = 99.99;  // to store lowest tariff & time slot present in available data
double lowestTariffTime = 0;
time_t startTimeArray[200];
time_t currentTime;
double currentTariff = 99.99;  // to hold live tariff for display
// Set LED Pins
int greenLEDPin = 19;
int redLEDPin = 18;
int boostButtonPin = 15;

const double tariffThreshold = 7.0;             // Tariff level below which the output pin (greenLEDPin) will be set LOW (Gas 9.84p / kWh at 6/1/2023)
long int lastTariffUpdate = 0;                  // used to store millis() of last tariff update
const long int tariffUpdateInterval = 3600000;  // millis() between successive tariff updates from Octopus (3600000ms = 1h, 10800s = 3h, 14400s = 4h)
int numRecords = 0;                             // No. of tariff records available from Octopus API
long int setOutputInterval = 45000;             // interval between checks of current tariff data against tariffThreshold
int d = 1;                                      // Counter for cycling the display content
//
WiFiClientSecure client;
JsonDocument doc;


void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  //TODO: arduino_event_info_t
  Serial.print("Connected to Wifi on: ");
  Serial.println(wifiSsid);
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Got DHCP IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(wifiSsid, wifiPassword);
}

//
void setup() {
  
  //Initialize serial and wait for port to open:
  Serial.begin(115200);

  Serial.println(F("Booty bootface..."));
  if (!headless)
  {
    display.cp437(true); //< Use correct character tables
    if ( !display.begin(SCLK_PIN, DIN_PIN, CS_PIN)) {
      Serial.println(F("ePaper allocation failed"));
      for (;;);  // Don't proceed, loop forever
    }
    // Clear the display buffer
    display.clearDisplay();
    // Display date of this code
    display.setTextColor(SCREEN_BLACK);
    display.setTextSize(3);  // Draw 2X-scale text
    display.setCursor(0, 0);
    display.print(firmwareDate);
    display.setCursor(0, 15);
    display.setTextSize(2);  // Draw 2X-scale text
    display.print(F("Starting.."));
    display.setCursor(0, 40);
    display.setTextSize(3);  // Draw 2X-scale text
    display.print(F("Moooo = "));
    display.print(tariffThreshold,2);
    display.print("p");

    display.display();  // Show initial text  
   display.waitForScreenBlocking(); //< Wait for prior update to complete
  }
  
  //delay(2000);
  //
  // Time Setup
  sntp_set_time_sync_notification_cb(timeavailable);
  /**
     NTP server address could be aquired via DHCP,

     NOTE: This call should be made BEFORE esp32 aquires IP address via DHCP,
     otherwise SNTP option 42 would be rejected by default.
     NOTE: configTime() function call if made AFTER DHCP-client run
     will OVERRIDE aquired NTP server address
  */
  sntp_servermode_dhcp(1);  // (optional)
  /**
     This will set configured ntp servers and constant TimeZone/daylightOffset
     should be OK if your time zone does not need to adjust daylightOffset twice a year,
     in such a case time adjustment won't be handled automagicaly.
  */
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);


  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  /* Remove WiFi event
  Serial.print("WiFi Event ID: ");
  Serial.println(eventID);
  WiFi.removeEvent(eventID);*/

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(wifiSsid);
  WiFi.begin(wifiSsid, wifiPassword);

  //TODO: WIFI.setAutoReconnect()
  //

 /* display.clearDisplay();
  //
  Serial.println("5555");
  display.setTextSize(2);  // Draw 2X-scale text
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(5, 0);
  display.println(F("SSID: "));
  display.println(F(wifiSsid));
  Serial.println("33333");


  display.display();  // Show initial text
   display.waitForScreenBlocking();
  */
}
//**************************************
void loop() {
  
  if ( WiFi.isConnected() )
  {
    //TODO: Should use NTC clock for daily fetch
    if (lastTariffUpdate == 0 
      || millis() > (lastTariffUpdate + tariffUpdateInterval))  // update Octopus tariff periodically
    {
      Get_Octopus_Data();
      if ( numRecords )
      {
        lastTariffUpdate = millis();
      }
    }
  }
  
  // TODO: better timed event
  if (lastTariffUpdate != 0 && (millis() % setOutputInterval) == 0)  // START OF TIMED LOOP : Code run each time tariff is checked & immersion output set
  {
    //
    //printLocalTime();  // it will take some time to sync time :)
    //
      //CHECK TARIFF AND SET OUTPUT PIN HERE
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      currentTime = mktime(&timeinfo);
      Serial.print("currentTime is ");
      Serial.println(currentTime);
      Serial.print("Number of Octopus Tariff Records = ");
      Serial.print(numRecords);
      Serial.print(", Tariff Threshold is ");
      Serial.println(tariffThreshold);
      int i = 0;
      lowestTariffVisible = 99.99;  // reset to 'crazy' values immediately before setting correctly
      lowestTariffTime = 0;
      while (i <= numRecords) {
        if ((currentTime > startTimeArray[i]) && ((currentTime - startTimeArray[i]) < 1800)) {
//        if (currentTime > (startTimeArray[i] + 1800)) {
          Serial.print("Current Tariff is ");
          // Serial.print(tariffArray[i], 2);
          currentTariff = tariffArray[i];
          Serial.print(currentTariff, 2);
          Serial.print("p (Record #");
          Serial.print(i);
          Serial.println(")");
          if (tariffArray[i] < tariffThreshold) {
          //  digitalWrite(greenLEDPin, HIGH);
            Serial.println("************** LOW TARIFF CONDITION **************");
          } else {
          //  digitalWrite(greenLEDPin, LOW);
          }
        }
        if ((tariffArray[i] < lowestTariffVisible) && (startTimeArray[i] + 1800) > currentTime)  // find lowest published tariff beyond present one
        {
          lowestTariffVisible = tariffArray[i];  // find lowest tariff present in available data
          lowestTariffTime = startTimeArray[i];
          Serial.print("Lowest tariff is ");
          Serial.println(lowestTariffVisible);
          Serial.print(", Array Values - ");
          Serial.print(tariffArray[i]);
          Serial.print(", ");
          Serial.println(startTimeArray[i]);
        }
        i++;
      }
      Serial.print("Lowest Future Tariff Published = ");
      Serial.println(lowestTariffVisible);
      Serial.print("Time to Lowest Tariff is ");
      Serial.print((lowestTariffTime - currentTime) / 3600);
      Serial.println(" h");
      //if (d == 1) {
        updateDisplay();
      //}
      //if (d == 2) {
      //  graphOLED();
      //}
      //d++;
      //if (d > 2) {
      //  d = 1;  //Reset display content counter
      //}

      display.
  }
} 

void Get_Octopus_Data()  // Get Octopus Data
{
  Serial.println("\nBegin get octopus data...");
  client.setCACert(octopus);
  Serial.println("\nStarting connection to Octopus server...");
  if (!client.connect(server, 443)) {
    Serial.println("Connection failed!");
//    digitalWrite(redLEDPin, HIGH);  // Set LED to indicate connection failure
  } else {
    Serial.println("Connected to server!");

 //   digitalWrite(redLEDPin, LOW);
    // Make a HTTP request:
    //client.println("GET https://api.octopus.energy/v1/products/AGILE-18-02-21/electricity-tariffs/E-1R-AGILE-18-02-21-E/standard-unit-rates/ HTTP/1.1");
    client.println("GET https://api.octopus.energy/v1/products/AGILE-FLEX-22-11-25/electricity-tariffs/E-1R-AGILE-FLEX-22-11-25-J/standard-unit-rates/ HTTP/1.1");
    client.println("Host: api.octopus.energy");
    client.println(auth_string); // enter Octopus authorisation string, collected from secrets.h
    client.println("Connection: close");
    client.println();
    
    //
    while (client.connected()) {
      String response = client.readStringUntil('\n');
        
        //Serial.println(response);
        
        /**Response starts with:
          Starting connection to Octopus server...
          Connected to server!
          HTTP/1.1 200 OK
          Date: Tue, 30 Jan 2024 20:32:57 GMT
          Content-Type: application/json
          Content-Length: 14095
          ...
          \r
        */
      if (response == "\r") {
        Serial.println("headers received");
        // TODO: CHeck the headers and read `Content-Length` etc
        break;
      }
    }

    // Wait for data bytes to be received
    while (client.connected() && !client.available() );

    // If there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) 
    {
      String line = client.readStringUntil('\n');
      // Serial.println(line);
      DeserializationError error = deserializeJson(doc, line);
      if (error) {
        Serial.print(F("deserializing JSON failed"));
        Serial.println(error.f_str());
        Serial.println("Here's the JSON I tried to parse");
        Serial.println(line);
      }
       else 
      {
        JsonArray results = doc["results"];
        numRecords = results.size();
        Serial.print("# of Records is ");
        Serial.println(numRecords);
        
        for ( int i =0; i < numRecords; ++i )
         {
          double tariff = doc["results"][i]["value_inc_vat"];
          tariffArray[i] = tariff;
          String periodStart = doc["results"][i]["valid_from"];
          struct tm tmpTime;
          strptime(periodStart.c_str(), "%Y-%m-%dT%H:%M:%SZ", &tmpTime);
          startTimeArray[i] = mktime(&tmpTime);

          Serial.print(tariff, 2);
          Serial.print("p, from ");
          //          Serial.println(periodStart);
          Serial.print(periodStart);
          Serial.print(" ");
          Serial.println(startTimeArray[i]);
          //          printTime(startTimeArray[i]);
        }
      }
      client.stop();
    }
  }

  
  Serial.println("DONE!?!?!?");
}
//
void printTime(time_t timeToPrint)  // Print time to serial monitor
{
  char buff1[100];
  struct tm tmpTime;
  gmtime_r(&timeToPrint, &tmpTime);
  strftime(buff1, 100, "%A, %B-%Y, %T", &tmpTime);
  Serial.print("buff1 = ");
  Serial.println(buff1);
}
// Clock Functions
//
/*
void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  //
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
//  display.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
*/
void updateDisplay() {
  if ( headless )
    return;

    
  Serial.print("Updating display...");

  display.clearDisplay();
  //
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No time available (yet)");
    return;
  }
  //
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //  display.print(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //
  display.setTextSize(3);
  display.setTextColor(SCREEN_BLACK);
  display.setCursor(0, 0);
  display.print(&timeinfo, "%A, %B %d %H:%M:%S");
  
  display.setCursor(0, 28);
  display.setTextSize(3);
  display.print("Current ");
  display.print(currentTariff);
  display.println(" p");
  
  display.print("Next Low");
  display.print(lowestTariffVisible);
  display.println(" p");

  display.print("In ");
  display.print(int((lowestTariffTime - currentTime) / 3600));
  display.print("h ");
  display.print(int((((lowestTariffTime - currentTime) / 3600) - int((lowestTariffTime - currentTime) / 3600)) * 60));
  display.println("m");

  display.display();
  
   display.waitForScreenBlocking(); //< Wait for prior update to complete
  
  Serial.print("Display update done ");
}
// Callback function (get's called when time adjusts via NTP)
//
void timeavailable(struct timeval* t) {
  Serial.println("Got time adjustment from NTP!");
  //  printLocalTime();
}
//
void graphOLED() {
  display.clearDisplay();
  //  display.drawLine(0, 15, 0, 63, SCREEN_BLACK);  // Draw Axes
  display.drawLine(0, 63, 127, 63, SCREEN_BLACK);
  int i = 1;
  while (i < 128) {
    if (i % 3 == 0) {
      display.drawPixel(i, 53, SCREEN_BLACK);  // Draw grid line at 10p intervals
      display.drawPixel(i, 43, SCREEN_BLACK);
      display.drawPixel(i, 33, SCREEN_BLACK);
    }
    i++;
  }  //
  i = 0;
  while (i <= numRecords) {
    if (currentTime < startTimeArray[i])  // only plot future values
    {
      if (startTimeArray[i] == lowestTariffTime) { // Draw circle around the lowest tariff visible, to highlight it
        display.fillCircle(((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), (63 - int(tariffArray[i])), 4, SCREEN_BLACK);
        display.fillCircle(((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), (63 - int(tariffArray[i])), 3, SCREEN_WHITE);
        // Change above to triangles?
        display.drawLine(((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), 63, ((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), (63 - int(tariffArray[i])), SCREEN_BLACK);
      } else {
        //      Serial.println(int(tariffArray[i]));
        //      display.drawLine((startTimeArray[i] - currentTime) / 900, 63, (startTimeArray[i] - currentTime) / 900, (63 - int(tariffArray[i])), SCREEN_BLACK);
        display.drawLine(((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), 63, ((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)), (63 - int(tariffArray[i])), SCREEN_BLACK);
        Serial.print(((startTimeArray[0] - currentTime) / 1800));
        Serial.print(" ");
        Serial.println(((startTimeArray[i] - currentTime) / 1800) * (127 / ((startTimeArray[0] - currentTime) / 1800)));
      }
    }
    i++;
    display.setTextSize(3);
    display.setTextColor(SCREEN_BLACK);
    display.setCursor(4, 0);
    display.print("Next Lowest ");
    //    display.setCursor(50, 57);
    display.print(lowestTariffVisible);
    display.print("p");
    display.setCursor(30, 12);
    //  display.print("Next Lowest ");
    // display.setCursor(50, 57);
    display.print("In ");
    display.print(int((lowestTariffTime - currentTime) / 3600));
    display.print("h ");
    display.print(int((((lowestTariffTime - currentTime) / 3600) - int((lowestTariffTime - currentTime) / 3600)) * 60));
    display.print("m");
  }
  display.display();      
  display.waitForScreenBlocking();
}