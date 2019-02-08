// debugging mode or not.
#define DEBUG true
#define WDEBUG false


// Sensor operations:
#include "sensoring.h"
// Communication operations:
#include "comm.h"


/*** LED Control ***/
#include "FastLED.h"
FASTLED_USING_NAMESPACE
#define DATA_PIN    13
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
#define NUM_LEDS   14
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
  burst,
  //degrow, // someone steps out of one altar
  //burst, // both altars filled long enough
  //rest, // pause after burst
  //emptying
  reset

};

// state machine variables:
state currentState = empty; // start with empty state
// general
bool slavePresence = false;
bool slaveOccupied = false;
bool masterOccupied = false;
// OnEnter:
bool gatesOff = false;
bool coreOn = false;
// Staying
int stayingCounter  = 0;
int stayingDelay = 50;
unsigned long previousStayingMillis = 0;
unsigned long previousStayingLightUpMillis = 0;
int stayingLightUpDuration = 3000;
// Emptying:
unsigned long previousEmptyingMillis  = 0;
int emptyingDelay = 30;
int emptyingAmount = 10;
int emptyingCounter = 0;
// Growing:
int growingCounter = 0;

unsigned long lastPrint = 0;
unsigned long sensorLastPrint = 0;







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
    fadeAmount[l] = 3;
  }

  // init altar before loop (used also as reset)
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
  // check sensor data, if present, update master occupied.
  slavePresence = sensoring();
  // listen to UDP messages, if message received, update slave occupied
  readUdp(); // updates variables and empty udp caChe.
  masterOccupied = isMasterOccupied();


  if (WDEBUG)
  {
    if (millis() - sensorLastPrint > 1000)
    {
      Console.print("Presence: ");
      Console.print(slavePresence);
      Console.print(" State: ");
      Console.print(currentState);
      Console.print(" Master Occupied: ");
      Console.println(masterOccupied);
      sensorLastPrint = millis();
    }
  }
  if (DEBUG)
  {
    if (millis() - sensorLastPrint > 1000)
    {
      Serial.print("Presence: ");
      Serial.print(slavePresence);
      Serial.print(" State: ");
      Serial.print(currentState);
      Serial.print(" Master Occupied: ");
      Serial.println(masterOccupied);
      sensorLastPrint = millis();
    }
  }




  switch (currentState)
  {
    case empty:
      // inform other that not occupied yet:
      sendUDP("em", IP_Master, master_port);
      // EMPTY CASE: LED 0 is lit up dimly, LED[1-2] off, LED[3] breathing

      // LIGHT CONTROL:
      // Core Lamp
      leds[0] = CHSV(rootHue, 255, ledBrightness[0]);
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;


      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      if (masterOccupied) breathingDelay = 1;
      else breathingDelay = 30;

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
      if (slavePresence)
      {
        currentState = onenter;
        if (WDEBUG) Console.println("switching state to OnEnter");
      }

      break;



    ///////////////////////////////////////////////////
    //                    ON ENTER                   //
    ///////////////////////////////////////////////////
    // the case when someone enters the altar.
    // it dims the gate lights and lits up Core lamp in a quick way.
    // once all lamps are ready, then next state is Staying.
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
    // the case when the person stays continuously in altar.
    // lights grow up and breathe
    case staying:
      // inform other that not occupied yet:
      sendUDP("em", IP_Master, master_port);
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
      // after certain seconds, led[1] start fading in, and then breathes.

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
          //// STATE CHANGE TO OCCUPIED////////////////////////
          currentState = occupied;
          if (WDEBUG) Console.println("switching state to Occupied");
          if (DEBUG) Serial.println("switching state to Occupied");
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

      //CONTROL:
      if (!slavePresence) // iin case the person leaves.
      {
        currentState = emptying;
        emptyingCounter = stayingCounter;
        if (WDEBUG) Console.println("switching state to Emptying");
        if (DEBUG) Serial.println("switching state to Emptying");
      }


      break;


    ///////////////////////////////////////////////////
    //                    Emptying                    //
    ///////////////////////////////////////////////////
    case emptying:
      //  empty all until base.
      // inform other that not occupied yet:
      sendUDP("em", IP_Master, master_port);
      // if the state comes from staying, then the stayingCounter keeps track.

      if ( (millis() - previousEmptyingMillis > emptyingDelay) )
      {

        //for (int k = stayingCounter; k >= 0; k--)
        if (emptyingCounter >= 0)
        {
          ledBrightness[emptyingCounter] -= emptyingAmount;
          if ( ledBrightness[emptyingCounter] <= 0)
          {
            ledBrightness[emptyingCounter] = 0;
            emptyingCounter--;
          }
          leds[emptyingCounter] = CHSV(rootHue, 255, ledBrightness[emptyingCounter]);
        }
        else
        {
          // STATE CHANGE:
          currentState = reset;
          if (WDEBUG) Console.println("switching state to Emptying");
          if (DEBUG) Serial.println("switching state to Emptying");
        }
        previousEmptyingMillis = millis();
      }

      break;


    ///////////////////////////////////////////////////
    //                    OCCUPIED                   //
    ///////////////////////////////////////////////////
    case occupied:
      // all are max brightness
      // all are breathing fast! waiting for other altar to get occupied.
      sendUDP("oc", IP_Master, master_port);

      breathingDelay = 10;
      if (millis() - previousBreathingMillis > breathingDelay)
      {
        for (int k = 0; k < NUM_ALTARLEDS; k++)
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
          leds[k] = CHSV(rootHue, 255, ledBrightness[k]);
        }
        previousBreathingMillis = millis();
      }

      if (masterOccupied)
      {
        //////////////////////STATE CHANGE////////////////////////////
        if (DEBUG) Serial.println("switching state to Grow");
        if (WDEBUG) Console.println("switching state to Grow");
        currentState = grow;
        growingCounter = NUM_ALTARLEDS;
      }
      if (!slavePresence)
      {
        //////////////////////STATE CHANGE////////////////////////////
        if (DEBUG) Serial.println("switching state to Empty");
        if (WDEBUG) Console.println("switching state to Empty");
        currentState = empty;
        emptyingCounter = NUM_ALTARLEDS;
      }

      break;




    ///////////////////////////////////////////////////
    //                    GROWING                    //
    ///////////////////////////////////////////////////
    case grow:

      if ( (millis() - previousStayingMillis > stayingDelay) )
      {

        for (int k = 0; k <= growingCounter; k++)
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
        growingCounter++;
        if ( growingCounter == NUM_LEDS)
        {
          ///////////////////////STATE CHANGE///////////////////////////////////
          currentState = burst;
          if (WDEBUG) Console.println("switching state to Burst");
          if (DEBUG) Serial.println("switching state to Burst");
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
      break;


    ///////////////////////////////////////////////////
    //                    BURST                      //
    ///////////////////////////////////////////////////
    case burst:
      // slave doesnt do anything.
      delay(1000);
      /////////////STATE CHANGE//////////
      if (WDEBUG) Console.println("switching state to Reset");
      if (DEBUG) Serial.println("switching state to Reset");
      currentState = reset;
      ;
      break;





    ///////////////////////////////////////////////////
    //                    RESET                      //
    ///////////////////////////////////////////////////
    case reset:
      // initialize variables.
      for (int l = 0; l < NUM_LEDS; l++)
      {
        ledBrightness[l] =  0;
        leds[l] = CHSV(rootHue, 255, ledBrightness[l]);
      }
      // Altar prepare
      ledBrightness[0] = coreInitBrightness;
      leds[0] = rootColor;
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;
      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      ledBrightness[3] = gateInitBrightness;
      leds[3] = rootColor;

      gatesOff = false;
      coreOn = false;
      stayingCounter = 0;

      // add also fade in to the empty state.

      /////////////STATE CHANGE//////////
      if (WDEBUG) Console.println("switching state to Empty");
      if (DEBUG) Serial.println("switching state to Empty");
      currentState = empty;
      break;

    default:
      ;
  }


  FastLED.show();
}
