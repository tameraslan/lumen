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
#define BRIGHTNESS     255 //150
#define FRAMES_PER_SECOND  120
#define ROOTCOLOR CRGB(155, 50, 5)
CRGB rootColor = CRGB(155, 50, 5);
uint8_t rootHue = 62; //220 243

#define NUM_ALTARLEDS 4
int gateInitBrightness = 255;
int coreInitBrightness = 127;

unsigned long previousBreathingMillis = 0;
int breathingDelay = 50;
int minBreathingBrightness = 100;
int maxBreathingBrightness = 250;
int fadeAmount = 5;





/*** State Machine ***/
enum state
{
  empty, //noone
  onenter, // prep for staying
  staying,
  occupied, // fully charged to grow
  grow, // both arduinos filled
  //degrow, // someone steps out of one altar
  //burst, // both altars filled long enough
  //rest, // pause after burst
  //emptying
  reset

};

state currentState = empty; // start with empty state
// state machine variables:
bool slavePresence = false;
bool slaveOccupied = false;
bool masterOccupied = false;

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
  slavePresence = sensoring();

  if (millis() - sensorLastPrint > 1000)
  {
    Console.print("Presence: ");
    Console.print(slavePresence);
    Console.print("State: ");
    Console.println(currentState);
    sensorLastPrint = millis();
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

      // check if master is occupied:
      readUdp(); // updates variables
      masterOccupied = isMasterOccupied();
      // Gate Lamps: breath normal when empty, faster when master is occupied:
      if (masterOccupied) breathingDelay = 5;
      else breathingDelay = 50;

      if (millis() - previousBreathingMillis > breathingDelay)
      {
        int tempBrightness = ledBrightness[3];
        tempBrightness += fadeAmount;
        // reverse the direction of the fading at the ends of the fade:
        if (tempBrightness <= minBreathingBrightness)
        {
          tempBrightness = minBreathingBrightness;
          fadeAmount = -fadeAmount ;
        }
        else if (tempBrightness >= maxBreathingBrightness)
        {
          tempBrightness = maxBreathingBrightness;
          fadeAmount = -fadeAmount ;
        }
        ledBrightness[3] = tempBrightness;
        leds[3] = CHSV(rootHue, 255, ledBrightness[3]);
        previousBreathingMillis = millis();
      }


      // STATE CONTROL:
      if (masterPresence)
      {
        currentState = onenter;
        Console.println("switching state to OnEnter");
      }

      break;

    case onenter:
      // increase brightness of Core Lamp

      ledBrightness[0] += 1;
      if (ledBrightness[0] >= 255)
      {
        ledBrightness[0] = 255;
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
        Console.println("switching state to Staying");
      }
      break;

    case staying:
      if (millis() - lastPrint > 500)
      {
        Console.print("Staying  ");
        Console.print(stayingCounter);
        Console.print(" ");
        Console.println(ledBrightness[stayingCounter]);
        lastPrint = millis();
      }
      
      if ( (millis() - previousStayingMillis > stayingDelay) )
      {
        if (ledBrightness[stayingCounter] < 255)
      {
        ledBrightness[stayingCounter] += 1;
        leds[stayingCounter] = CHSV(rootHue, 255, ledBrightness[stayingCounter]);
      }
      else
      {
        ledBrightness[stayingCounter] = 255;
        //
      }
        
        }

      
      


      if ( (millis() - previousStayingLightUpMillis > stayingLightUpDuration) &&
           (ledBrightness[stayingCounter] == 255))
      {
        stayingCounter++;
        previousStayingLightUpMillis = millis();
      }

      // CONTROL:
      //      if (!masterPresence) // if they leave
      //      {
      //        currentState = emptying;
      //        Console.println("switching state to Emptying");
      //      }

      if ( stayingCounter == NUM_ALTARLEDS)
      {
        currentState = occupied;
        Console.println("switching state to Occupied");
      }
      break;

    case occupied:
    
      // all are max brightness
      // all are breathing fast!
      breathingDelay = 10;
      if (millis() - previousBreathingMillis > breathingDelay)
      {
        for(int k = 0; k<NUM_ALTARLEDS; k++)
        {
          ledBrightness[k] += fadeAmount;
        // reverse the direction of the fading at the ends of the fade:
        
        if (ledBrightness[k] <= minBreathingBrightness)
        {
          ledBrightness[k] = minBreathingBrightness;
          fadeAmount = -fadeAmount ;
        }
        else if (ledBrightness[k] >= maxBreathingBrightness)
        {
          ledBrightness[k] = maxBreathingBrightness;
          fadeAmount = -fadeAmount ;
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
