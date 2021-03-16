#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <SPI.h>
#include "EspMQTTClient.h"

#include <Adafruit_GFX.h>
#include "Max72xxPanel.h"

#define MAX_DEVICES 4 // your device count

//################# LIBRARIES ##########################
#ifdef ESP8266
 #include <ESP8266WiFi.h>
 #include <ESP8266WebServer.h>
 int    pinCS = D4;                      // Attach CS to this pin, DIN to MOSI and CLK to SCK
 //     MOSI  = D7;                      // Attach MOSI on Display to this pin
 //     CLK   = D5;                      // Attach CLK on Display to this pin
#else
 #include <WiFi.h>
 #include <ESP32WebServer.h>
 int    pinCS = 5;                      // Attach CS to this pin, DIN to MOSI and CLK to SCK
 //     MOSI  = 23;                     // Attach MOSI on display to this pin
 //     MOSI  = 18;                     // Attach CLK on display to this pin
#endif

int wait   = 75; // In milliseconds between scroll movements
int spacer = 1;
int width  = 5 + spacer; // The font width is 5 pixels
String SITE_WIDTH =  "1000";

int inMeeting = 0;
int meetingLoop = 0;
int smileyOffset = 0;

int    numberOfHorizontalDisplays = 4;
int    numberOfVerticalDisplays   = 1;
char   time_value[20];
String message, message1;

Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

#include <credentials.h>

enum displayMode {
  DISPLAY_SCROLL,
  DISPLAY_MEETING,
  DISPLAY_SMILEY,
  DISPLAY_IDLE
};

displayMode currentDisplayMode = DISPLAY_SCROLL;

const char* ssid = MYSSID;
const char* pwd = MYPWD;
const char* mqttUser = MYMQTTUSER;
const char* mqttPasswd = MYMQTTPWD;

EspMQTTClient client(
  ssid,
  pwd,
  "192.168.88.88",  // MQTT Broker server ip
  mqttUser,   // Can be omitted if not needed
  mqttPasswd,   // Can be omitted if not needed
  "MAX7219_DISPLAY",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);

void setup();
void onConnectionEstablished();
void display_in_meeting();
void clear_screen();
void display_message(String message);
void loop();

void setup()
{
  currentDisplayMode = DISPLAY_SCROLL;

  Serial.begin(115200);
  while (!Serial) {}
  Serial.println();
  matrix.setIntensity(2);
  matrix.setRotation(0, 1);  // The first display is position upside down
  matrix.setRotation(1, 1);  // The first display is position upside down
  matrix.setRotation(2, 1);  // The first display is position upside down
  matrix.setRotation(3, 1);  // The first display is position upside down
  
  client.enableHTTPWebUpdater("/");

  Serial.print("Connecting...");
}

void onConnectionEstablished()
{
  display_message("Connected!"); // Display the message
// MQTT subscription topic for display text
  client.subscribe("leddisplay/text", [](const String & payload) {
    Serial.println("Got new message, switching to scroll display");
    currentDisplayMode = DISPLAY_SCROLL;
    message = payload.c_str();
  });
  
  // MQTT subscription topic for display intensity controll
  client.subscribe("leddisplay/intensity", [](const String & payload) {
    matrix.setIntensity(payload.toInt());    // Use a value between 0 and 15 for brightness
  });

// MQTT subscription topic for display scroll speed controll
  client.subscribe("leddisplay/scroll", [](const String & payload) {
    wait = payload.toInt();
  });

  client.subscribe("leddisplay/smiley", [](const String & payload) {
    currentDisplayMode = DISPLAY_SMILEY;
    Serial.println("Showing smiley!");
  });

  client.subscribe("leddisplay/meeting", [](const String & payload) {
    inMeeting = payload.toInt();
    if (inMeeting > 0) {
      Serial.println("Switching to meeting display");
      currentDisplayMode = DISPLAY_MEETING;  
    } else {
      Serial.println("Switching to idle mode");
      currentDisplayMode = DISPLAY_IDLE;
      clear_screen();
    }
  });
}

void display_in_meeting() {
  int y = (matrix.height() - 8) / 2; // center the text vertically

  matrix.setCursor(5,y);
  meetingLoop = !meetingLoop;

  if (meetingLoop) {
    matrix.fillScreen(LOW);
    matrix.setTextColor(HIGH, LOW);
  } else {
    matrix.fillScreen(HIGH);
    matrix.setTextColor(LOW, HIGH);
  }

  matrix.print("BUSY");
  matrix.write();

  delay(wait * 10);
}

void clear_screen() {
  matrix.fillScreen(LOW);
  matrix.write();
}

void display_message(String message) {
  // This allows us to get out of the loop early if a new message was received.
  if (currentDisplayMode != DISPLAY_SCROLL) {
    return;
  }

  Serial.println(message);
  for ( int i = 0 ; i < width * (int)message.length() + matrix.width() - spacer; i++ ) {
    //matrix.fillScreen(LOW);
    int letter = i / width;
    int x = (matrix.width() - 1) - i % width;
    int y = (matrix.height() - 8) / 2; // center the text vertically
    while ( x + width - spacer >= 0 && letter >= 0 ) {
      if ( letter < (int)message.length() ) {
        matrix.drawChar(x, y, message[letter], HIGH, LOW, 1); // HIGH LOW means foreground ON, background OFF, reverse these to invert the display!
      }
      letter--;
      x -= width;
    }
    matrix.write(); // Send text to display
    delay(wait / 2);
    client.loop();
  }
}

void display_smiley() {
  clear_screen(); 

  int whichDisplay = 1;
  int radius = 3;

  matrix.drawCircle(whichDisplay*8+radius,1+radius, radius, HIGH);
  matrix.drawPixel(whichDisplay*8+2+smileyOffset-1, 3, HIGH);
  matrix.drawPixel(whichDisplay*8+4+smileyOffset-1, 3, HIGH);

  matrix.drawPixel(whichDisplay*8+3+smileyOffset-1, 5, HIGH);
  matrix.write();

  delay(wait * 10);

  smileyOffset++;
  if (smileyOffset > 2)  {
    smileyOffset = 0;
  }
}

void loop()
{
  client.loop();
  switch (currentDisplayMode) {
    case DISPLAY_SCROLL: display_message(message); break; // Display the message; break
    case DISPLAY_MEETING: display_in_meeting(); break;
    case DISPLAY_SMILEY: display_smiley(); break;
    default: delay(1000); break;
  } 
}
