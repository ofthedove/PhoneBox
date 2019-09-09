#include<Servo.h>
#include "SSD1306Wire.h"
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define BUTTON 12
#define KNOB A0
#define SERVO 16
#define DISPLAY1 5
#define DISPLAY2 4
#define DISPLAY_ADDR 0x3c

const char* ssid = "network";
const char* pw = "ofthedove";
int utc = -4; // Eastern daylight time
WiFiClient client;
WiFiUDP udp;
NTPClient timeClient(udp, "time.nist.gov", utc * 3600, 60000);

enum {
  latchUnlocked = 45,
  latchLocked = 135,
};

bool buttonPressed = false;
bool armed = false;
String armedString = "Open";
unsigned int knobValue = 0;
String timeString = "";
String alarmString = "";


Servo latch;
SSD1306Wire  display(DISPLAY_ADDR, DISPLAY1, DISPLAY2);

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pw);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();
  
  latch.attach(SERVO);
  latch.write(latchUnlocked);  //Set to unlocked
  
  pinMode(0, OUTPUT);
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_24);
  display.drawString(0, 0, "Startup");
  display.display();

  pinMode(BUTTON, INPUT_PULLUP);
}

void DisplayTask()
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(63,0, timeString);
  display.drawString(63,20, alarmString);
  display.drawString(63,40, armedString);
  display.display();
}

void TimeTask()
{
  timeClient.update();
  String timeOfDay = timeClient.getFormattedTime();
  Serial.println(timeOfDay);
  timeString = timeOfDay;

  if (alarmString == timeString) {
    armed = false;
    armedString = "Open";
  }
}

void KnobTask()
{
  unsigned int potential = analogRead(KNOB);
  knobValue = map(potential, 0, 024, 1440, 0);

  static bool prevArmed = false;
  if (armed && armed == prevArmed) {
    return;
  }
  prevArmed = armed;

  knobValue = potential;
  int hour = knobValue / 60;
  int min = knobValue % 60;
  alarmString = String(hour) + ":" + String(min) + ":00";
}

void ServoTask()
{
  static bool prevArmed;
  if (armed == prevArmed) {
    return;
  }
  prevArmed = armed;
  
  if (armed) {
    latch.write(latchLocked);
  } else {
    latch.write(latchUnlocked);
  }
}

void ButtonTask()
{
  // True is released, false is pressed
  bool buttonState = digitalRead(BUTTON);
  buttonPressed = !buttonState;

  static bool prevButtonState = false;
  static bool armedB = false;
  if ((buttonState != prevButtonState) && (buttonState == false)) {
    armedB = !armedB;
    if(armedB) {
      armed = true;
      armedString = "Locked";
    } else {
      armed = false;
      armedString = "Open";
    }
  }

  prevButtonState =buttonState;
}

// Scheduler ----------------------------
typedef struct
{
  unsigned long previousMillis;
  unsigned long elapsedMillis;
  unsigned long timeoutMillis;
  void (*callback)();
} Timer_t;

static Timer_t schedulerTable[] = 
{
  {0, 0, 100, &ButtonTask},
  {0, 0, 100, &KnobTask},
  {0, 0, 100, &DisplayTask},
  {0, 0, 500, &TimeTask},
  {0, 0, 100, &ServoTask}
};

void runScheduler()
{ 
  // Run each timer in the scheduler table, and call 
  for (int i = 0; i < sizeof(schedulerTable)/sizeof(Timer_t); i++)
  {
    // Note: millis() will overflow after ~50 days.  
    unsigned long currentMillis = millis();
    Timer_t *t = &schedulerTable[i];    
    t->elapsedMillis += currentMillis - t->previousMillis;
    t->previousMillis = currentMillis;
    if (t->elapsedMillis >= t->timeoutMillis)
    {
      t->elapsedMillis = 0;
      t->callback();
    }
  }
}

void loop() {
  runScheduler();
}
