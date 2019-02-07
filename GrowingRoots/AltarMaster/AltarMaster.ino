// debugging mode or not.
#define DEBUG false
#define WDEBUG true


// Sensor operations:
#include "sensoring.h"
// Communication operations:
#include "comm.h"


/*** LED Control ***/
#include "FastLED.h"
FASTLED_USING_NAMESPACE
#define DATA_PIN    13
#define LED_TYPE    WS2812B
#define COLOR_ORDER RGB
#define NUM_LEDS   17
CRGB leds[NUM_LEDS];
int ledBrightness[NUM_LEDS];
int fadeAmount[NUM_LEDS];
#define BRIGHTNESS     255 //150
#define FRAMES_PER_SECOND  120
#define ROOTCOLOR CRGB(155, 50, 5)
CRGB rootColor = CRGB(155, 50, 5);
uint8_t rootHue = 64; //220 243

#define NUM_ALTARLEDS 4
int gateInitBrightness = 255;
int coreInitBrightness = 127;

unsigned long previousBreathingMillis = 0;
int breathingDelay = 50;
int minBreathingBrightness = 100;
int maxBreathingBrightness = 255;
int breathingAmount = 5;





/*** State Machine ***/
enum state
{
  empty, //noone
  onenter, // prep for staying
  staying,
  occupied, // fully charged to grow
  emptying,
  grow, // both arduinos filled
  //degrow, // someone steps out of one altar
  //burst, // both altars filled long enough
  //rest, // pause after burst
  //emptying
  reset

};

state currentState = empty; // start with empty state
// state machine variables:
bool masterPresence = false;
bool masterOccupied = false;
bool slaveOccupied = false;

int stayingCounter  = 0;

int stayingDelay = 50;
unsigned long previousStayingMillis = 0;
unsigned long previousStayingLightUpMillis = 0;
int stayingLightUpDuration = 10000;

int growCounter = 0;
unsigned long lastPrint = 0;
unsigned long sensorLastPrint = 0;
bool gatesOff = false;
bool coreOn = false;






void setup()
{
  delay(1000); // whait till everything is ready

  // Sensors init
  for (int i = 0; i < NUM_SENSORS; i++)
  {
    captureVoltage[i] = thresholds[i]; // initialize capture values
  }


  // LEDS init
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); // initialize leds
  FastLED.setBrightness(BRIGHTNESS); // initialize led brightness
  FastLED.showColor(CRGB(0, 0, 0));  // all LEDs off


  // Comm init
  Bridge.begin();        // initialize Bridge
  Udp.begin(local_port); // start listening to local port

  // Debugging:
  if (DEBUG) Serial.begin(115200);
  if (WDEBUG)
  {
    Console.begin();
    while (!Console) {
      ; // wait for Console port to connect.
    }
    Console.println("You're connected to the Console!!!!");
  }

  // setup initial state colors already?
  // init all as off
  for (int l = 0; l < NUM_LEDS; l++)
  {
    ledBrightness[l] =  0;
    fadeAmount[l] = 5;
  }
  // Core Lamp

  ledBrightness[0] = coreInitBrightness;
  leds[0] = rootColor;
  // back LEDs
  leds[1] = CRGB::Black;
  leds[2] = CRGB::Black;
  // Gate Lamps: breath normal when empty, faster when slave is occupied:
  ledBrightness[3] = gateInitBrightness;
  leds[3] = rootColor;
  FastLED.show();

}



void loop()
{
  // update info on both altars if they are empty or full:
  // listen to UDP messages, if message received, update slave occupied
  //readUdp();
  // check sensor data, if present, update master occupied.
  masterPresence = sensoring();

  if (WDEBUG)
  {
    if (millis() - sensorLastPrint > 3000)
    {
      Console.print("Presence: ");
      Console.print(masterPresence);
      Console.print("State: ");
      Console.println(currentState);
      sensorLastPrint = millis();
    }

  }


  switch (currentState)
  {
    case empty:
      // EMPTY CASE: LED 0 is lit up dimly, LED[1-2] off, LED[3] breathing

      // LIGHT CONTROL:
      // Core Lamp
      leds[0] = CHSV(rootHue, 255, ledBrightness[0]);
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;
      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      if (slaveOccupied) breathingDelay = 10;
      else breathingDelay = 50;

      if (millis() - previousBreathingMillis > breathingDelay)
      {
        int tempBrightness = ledBrightness[3];
        tempBrightness += fadeAmount[3];
        // reverse the direction of the fading at the ends of the fade:
        if (tempBrightness <= minBreathingBrightness)
        {
          tempBrightness = minBreathingBrightness;
          fadeAmount[3] = -fadeAmount[3] ;
        }
        else if (tempBrightness >= maxBreathingBrightness)
        {
          tempBrightness = maxBreathingBrightness;
          fadeAmount[3] = -fadeAmount[3] ;
        }
        ledBrightness[3] = tempBrightness;
        leds[3] = CHSV(rootHue, 255, ledBrightness[3]);
        previousBreathingMillis = millis();
      }


      // STATE CONTROL:
      if (masterPresence)
      {
        currentState = onenter;
        if (WDEBUG) Console.println("switching state to OnEnter");
      }

      break;


    ///////////////////////////////////////////////////
    //                    ON ENTER                   //
    ///////////////////////////////////////////////////
    case onenter:
      // increase brightness of Core Lamp

      ledBrightness[0] += 1;
      if (ledBrightness[0] >= maxBreathingBrightness)
      {
        ledBrightness[0] = maxBreathingBrightness;
        coreOn = true;
      }
      leds[0] = CHSV(rootHue, 255, ledBrightness[0]);

      // decrease brightness of gate Lamps
      ledBrightness[3] -= 1;
      if (ledBrightness[3] <= 0)
      {
        ledBrightness[3] = 0;
        gatesOff = true;
      }
      leds[3] = CHSV(rootHue, 255, ledBrightness[3]);

      if (gatesOff && coreOn)
      {
        currentState = staying;
        previousStayingLightUpMillis = millis();
        if (WDEBUG) Console.println("switching state to Staying");
      }
      break;


    ///////////////////////////////////////////////////
    //                    STAYING                    //
    ///////////////////////////////////////////////////
    case staying:
      // print current counter values
      if (WDEBUG)
      {
//        if (millis() - lastPrint > 50)
//        {
//          Console.print("Staying: ");
//          Console.print(stayingCounter);
//          Console.print("   ");
//          Console.print("P0: ");
//          Console.print(ledBrightness[0]);
//          Console.print(":");
//          Console.print(fadeAmount[0]);
//          Console.print("   ");
//          Console.print("P1: ");
//          Console.print(ledBrightness[1]);
//          Console.print(":");
//          Console.print(fadeAmount[1]);
//          Console.print("   ");
//          Console.print("P2: ");
//          Console.print(ledBrightness[2]);
//          Console.print(":");
//          Console.print(fadeAmount[2]);
//          Console.print("   ");
//          Console.print("P3: ");
//          Console.print(ledBrightness[3]);
//          Console.print(":");
//          Console.print(fadeAmount[3]);
//          Console.println("   ");
//
//          lastPrint = millis();
//        }
      }

      // first core is lit, it breathes, others off.
      // after ten seconds, led[1] start fading in, and then breathes.

      // increase brightness of next point.
      if ( (millis() - previousStayingMillis > stayingDelay) )
      {

        for (int k = 0; k <= stayingCounter; k++)
        {
          ledBrightness[k] += fadeAmount[k];
          if (ledBrightness[k] <= minBreathingBrightness)
          {
            ledBrightness[k] = minBreathingBrightness;
            fadeAmount[k] = -fadeAmount[k] ;
          }
          else if (ledBrightness[k] >= maxBreathingBrightness)
          {
            ledBrightness[k] = maxBreathingBrightness;
            fadeAmount[k] = -fadeAmount[k] ;
          }
          //if(k<=stayingCounter)
          leds[k] = CHSV(rootHue, 255, ledBrightness[k]);
        }
        previousStayingMillis = millis();
      }





      if ( (millis() - previousStayingLightUpMillis > stayingLightUpDuration))
      {
        stayingCounter++;
        if ( stayingCounter == NUM_ALTARLEDS)
        {
          currentState = occupied;
          if(WDEBUG) Console.println("switching state to Occupied");
          break;
        }
        fadeAmount[stayingCounter] = fadeAmount[stayingCounter - 1];
        // bring next pixel to the prev pix val fast
        while (ledBrightness[stayingCounter] != ledBrightness[stayingCounter - 1])
        {
          ledBrightness[stayingCounter]++;
          leds[stayingCounter] = CHSV(rootHue, 255, ledBrightness[stayingCounter]);
          delay(3);
          FastLED.show();
        }
        

        previousStayingLightUpMillis = millis();
      }

      // CONTROL:
      //      if (!masterPresence) // if they leave
      //      {
      //        currentState = emptying;
      //        Console.println("switching state to Emptying");
      //      }


      break;

    case emptying:
      ;
      break;

    case occupied:

      // all are max brightness
      // all are breathing fast!
      breathingDelay = 10;
      if (millis() - previousBreathingMillis > breathingDelay)
      {
        for (int k = 0; k < NUM_ALTARLEDS; k++)
        {
          ledBrightness[k] += fadeAmount[k];
          // reverse the direction of the fading at the ends of the fade:

          if (ledBrightness[k] <= minBreathingBrightness)
          {
            ledBrightness[k] = minBreathingBrightness;
            fadeAmount[k] = -fadeAmount[k] ;
          }
          else if (ledBrightness[k] >= maxBreathingBrightness)
          {
            ledBrightness[k] = maxBreathingBrightness;
            fadeAmount[k] = -fadeAmount[k] ;
          }

          leds[k] = CHSV(rootHue, 255, ledBrightness[k]);

        }


        previousBreathingMillis = millis();
      }

      if (slaveOccupied)
        currentState = grow;
      if (!masterPresence)
        currentState = reset;
      break;

    case grow:
      leds[0] = CRGB::Red;
      //Console.println("grow");
      currentState = reset;
      break;


    case reset:
      //leds[1] = CRGB::Red;
      //delay(5000);
      // initialize variables.


      for (int l = 0; l < NUM_LEDS; l++)
      {
        ledBrightness[l] =  0;
      }

      ledBrightness[0] = coreInitBrightness;
      leds[0] = rootColor;
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;
      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      ledBrightness[3] = gateInitBrightness;
      leds[3] = rootColor;
      currentState = empty;
      gatesOff = false;
      coreOn = false;
      stayingCounter = 0;
      break;

    default:
      ;
  }


  FastLED.show();
}
