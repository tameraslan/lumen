// sensor testing by lighting up correspong lamps with sensors.
// debugging mode or not.
#define DEBUG true


#include "FastLED.h"
FASTLED_USING_NAMESPACE
#define DATA_PIN    6
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    40
CRGB leds[NUM_LEDS];
#define BRIGHTNESS          255
#define FRAMES_PER_SECOND  120

// how many sensor are connected?
#define NUM_SENSORS 4
// filtered sensor data is kept here:
float sensorVoltage[NUM_SENSORS];

// Declaration
//  bool* capture = 0;
//  int sensorArraySize = NUM_SENSORS;
//


bool capture[NUM_SENSORS];
long unsigned int captureTime[NUM_SENSORS];


// Filtering variables (averaging)
#define NUM_READINGS 32
int readings[NUM_SENSORS][NUM_READINGS];
long unsigned int runningTotal[NUM_SENSORS];                  // the running total
int average[NUM_SENSORS];
// init of op-var
int index = 0;                  // the index of the current reading
unsigned long prevMillis = 0;
// filtering variables:
int sensorReadingInterval = 55;

int captureDuration = 1000;
float captureVoltage = 1.2;

// sensor connections:
// static const uint8_t analog_pins[] = {A0,A1,A2,A3,A4,A5,A6};


void setup() {
  delay(1000); // 3 second delay for recovery
  Serial.begin(115200);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}



void loop()
{
  // SENSOR READ AND AVERAGING
  // index goes around the array in circular fashion:
  // once it records, it moves to the next value which at that moment keeps the last value in the array.
  // read every certain interval
  if (millis() - prevMillis > sensorReadingInterval)
  {

    // subtract the last reading:
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      runningTotal[t] = runningTotal[t]  - readings[t][index];
    }

    // read from the sensor:
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      readings[t][index] = analogRead(t);
    }

    // add the reading to the total:
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      runningTotal[t] = runningTotal[t] + readings[t][index];
    }

    // advance to the next position in the array:
    index = index + 1;
    // if we're at the end of the array...
    if (index >= NUM_READINGS) index = 0; // ...wrap around to the beginning:
    // calculate the average:
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      average[t] = runningTotal[t] / NUM_READINGS;
    }

    // convert  to voltage
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      sensorVoltage[t] = float(average[t]) / 1024.0 * 5.0;
      //if(DEBUG) Serial.print(sensorVoltage[t]);
    }


    prevMillis = millis();
  }


  // SENSOR CAPTURE: IF SENSOR DETECT AN OBJECT, THEY REMAIN TRIGGERED FOR A PREDEFINED TIME
  for (int s = 0; s < NUM_SENSORS; s++)
  {
    if (sensorVoltage[s] > captureVoltage)
    {
      capture[s] = true;
      leds[s] = CRGB::White;
      // record time
      captureTime[s] = millis();
    }
    else
    {
      if ((millis() - captureTime[s]) > captureDuration)
      {
        capture[s] = false;
        leds[s] = CRGB::Black;
      }
    }
  }

  // if any of the sensors detect someone, it means there is presence
  bool presence = false;
  for (int t = 0; t < NUM_SENSORS; t++)
  {
    presence = presence || capture[t];  
    
  }

  
  
  if (DEBUG)
    {
      for (int s = 0; s < NUM_SENSORS; s++)
      {
        Serial.print("S");
        Serial.print(s);
        Serial.print(": ");
        Serial.print(capture[s]);
        Serial.print("    ");
      }
      Serial.println();
      if(presence) Serial.println("altar occupied");
    }

  
  // send the 'leds' array out to the actual LED strip
  FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);

}


