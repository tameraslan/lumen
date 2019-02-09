// debugging mode or not.
#define DEBUG false
#define WDEBUG true



// is this arduino master or slave???
#define MASTER true

// Sensor operations:
#include "sensoring.h"
// Communication operations:
#include "comm.h"
unsigned long lastMessageSent = 0;
int messageFrequency = 1000;

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

int minSleepingBrightness = 0;
int maxSleepingBrightness = 63;




/*** State Machine ***/
enum state
{
  empty, //noone
  onenter, // prep for staying
  staying,
  occupied, // fully charged to grow
  emptying, //empty from the last pixel lit up
  grow, // both arduinos filled
  burst, // both altars filled long enough
  resting, // pause after burst
  reset  //reset all variables

};

// state machine variables:
state currentState = reset; // start with empty state
// general
bool presence = false;
//bool slaveOccupied = false;
bool otherOccupied = false;
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
// resting:
unsigned long restEnter = 0;
int restingTime = 30000;
unsigned long previousRestingMillis = 0;
int restingDelay = 50;

unsigned long lastPrint = 0;
unsigned long sensorLastPrint = 0;
unsigned long lastReadUDPtime = 0;
int readUDPDelay = 500;


void setup()
{

  delay(1000); // whait till everything is ready
  // Debugging:
  if (DEBUG) 
  {
    Serial.begin(115200);
    
  }
  
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
  setComms(MASTER); // first define which ip and ports should be taken
  if(DEBUG) 
  {
    Serial.println("local port: ");
    Serial.println(local_port);
  }
    
    
  Bridge.begin();        // initialize Bridge
  //int local_port = getLocalPort();
  Udp.begin(local_port); // start listening to local port
  
  
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
//  for (int l = 0; l < NUM_LEDS; l++)
//  {
//    ledBrightness[l] =  0;
//    fadeAmount[l] = 3;
//  }

//  // init altar before loop (used also as reset)
//  ledBrightness[0] = coreInitBrightness;
//  leds[0] = rootColor;
//  // back LEDs
//  leds[1] = CRGB::Black;
//  leds[2] = CRGB::Black;
//  // Gate Lamps: breath normal when empty, faster when slave is occupied:
//  ledBrightness[3] = gateInitBrightness;
//  leds[3] = rootColor;
//  FastLED.show();
}



void loop()
{
  // update info on both altars if they are empty or full:
  // check sensor data, if present, update master occupied.
  presence = sensoring();
  // listen to UDP messages, if message received, update slave occupied
//  if(millis()-lastReadUDPtime > readUDPDelay)
//  {
    
   bool control = readUdp(); // updates variables and empty udp caChe.
//   if(WDEBUG) Console.print("Message Read: ");
//    if(WDEBUG) Console.println(control);
//    lastReadUDPtime = millis();
//  }
  otherOccupied = isOtherOccupied();


  if (WDEBUG)
  {
    if (millis() - sensorLastPrint > 1000)
    {
      Console.print("Presence: ");
      Console.print(presence);
      Console.print(" State: ");
      Console.print(currentState);
      Console.print(" Other Occupied: ");
      Console.println(otherOccupied);
      sensorLastPrint = millis();
    }
  }
  if (DEBUG)
  {
    if (millis() - sensorLastPrint > 1000)
    {
      Serial.print("Presence: ");
      Serial.print(presence);
      Serial.print(" State: ");
      Serial.print(currentState);
      Serial.print(" Other Occupied: ");
      Serial.println(otherOccupied);
      sensorLastPrint = millis();
    }
  }




  switch (currentState)
  {
    ///////////////////////////////////////////////////
    //                    RESET                      //
    ///////////////////////////////////////////////////
    case reset:
//            if(millis()-lastMessageSent > messageFrequency)
//      {
//        //if(DEBUG) Serial.print("sending message");
//        sendUDP("em", IP_Other, other_port);
//        lastMessageSent = millis();
//        }
      // initialize variables.
      for (int l = 0; l < NUM_LEDS; l++)
      {
        ledBrightness[l] =  0;
        leds[l] = CHSV(rootHue, 255, ledBrightness[l]);
        fadeAmount[l] = 3;
      }
      // Altar prepare
      //ledBrightness[0] = coreInitBrightness;
      //leds[0] = rootColor;
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;
      //leds[3] = rootColor;
      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      //ledBrightness[3] = gateInitBrightness;
      for (int t = ledBrightness[0]; t<coreInitBrightness;t++)
      {
        ledBrightness[0] = t;
        leds[0] = CHSV(rootHue, 255, ledBrightness[0]);
        FastLED.show();
        //delay(10);
       }
       
      for (int t = ledBrightness[3]; t<gateInitBrightness;t++)
      {
        leds[3] = CHSV(rootHue, 255, ledBrightness[3]);
        FastLED.show();
        //delay(10);
       } 
      
      
      
      gatesOff = false;
      coreOn = false;
      stayingCounter = 0;

      // add also fade in to the empty state.

      
      /////////////STATE CHANGE//////////
      if (WDEBUG) Console.println("switching state to Empty");
      if (DEBUG) Serial.println("switching state to Empty");
      currentState = empty;
      sendUDP("em", IP_Other, other_port);
      break;

      
    ///////////////////////////////////////////////////
    //                    EMPTY .                    //
    ///////////////////////////////////////////////////
    case empty:
      // inform other that not occupied yet:
//      if(millis()-lastMessageSent > messageFrequency)
//      {
//        //if(DEBUG) Serial.print("sending message");
//        sendUDP("em", IP_Other, other_port);
//        lastMessageSent = millis();
//        }

      // EMPTY CASE: LED 0 is lit up dimly, LED[1-2] off, LED[3] breathing

      // LIGHT CONTROL:
      // Core Lamp
      leds[0] = CHSV(rootHue, 255, ledBrightness[0]);
      // back LEDs
      leds[1] = CRGB::Black;
      leds[2] = CRGB::Black;


      // Gate Lamps: breath normal when empty, faster when slave is occupied:
      if (otherOccupied) breathingDelay = 1;
      else breathingDelay = 30;

      if (millis() - previousBreathingMillis > breathingDelay)
      {
//        if(millis()-previousBreathingMillis > 100)
//        {
//          if(DEBUG) Serial.print(millis()-previousBreathingMillis);
//          if(DEBUG) Serial.print("  ");
//          }
        
        //
        
        
        //int tempBrightness = 
        ledBrightness[3] += fadeAmount[3];
        // reverse the direction of the fading at the ends of the fade:
        if (ledBrightness[3] <= minBreathingBrightness)
        {
          ledBrightness[3] = minBreathingBrightness;
          fadeAmount[3] = -fadeAmount[3] ;
          //if(DEBUG) Serial.println();
        }
        else if (ledBrightness[3] >= maxBreathingBrightness)
        {
          ledBrightness[3] = maxBreathingBrightness;
          fadeAmount[3] = -fadeAmount[3] ;
          
        }
        leds[3] = CHSV(rootHue, 255, ledBrightness[3]);
        previousBreathingMillis = millis();
      }


      // STATE CONTROL:
      if (presence)
      {
        /////////  STATE CHANGE .  ///////// 
        currentState = onenter;
        if (WDEBUG) Console.println("switching state to OnEnter");
        if (DEBUG) Serial.println("switching state to OnEnter");
        sendUDP("em", IP_Other, other_port);
      }
//if(DEBUG) Serial.print(" 6: ");
//      if(DEBUG) Serial.print(millis()); 
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
        /////////  STATE CHANGE .  ///////// 
        currentState = staying;
        previousStayingLightUpMillis = millis();
        sendUDP("em", IP_Other, other_port);
        if (WDEBUG) Console.println("switching state to Staying");
        if (DEBUG) Serial.println("switching state to Staying");
      }
      break;


    ///////////////////////////////////////////////////
    //                    STAYING                    //
    ///////////////////////////////////////////////////
    // the case when the person stays continuously in altar.
    // lights grow up and breathe
    case staying:
      // inform other that not occupied yet:
//      if(millis()-lastMessageSent > messageFrequency)
//      {
//        sendUDP("em", IP_Other, other_port);
//        lastMessageSent = millis();
//        }
      
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
          sendUDP("oc", IP_Other, other_port);
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
      if (!presence) // iin case the person leaves.
      {
        /////////  STATE CHANGE .  ///////// 
        currentState = emptying;
        emptyingCounter = stayingCounter;
        sendUDP("em", IP_Other, other_port);
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
//      if(millis()-lastMessageSent > messageFrequency)
//      {
//        sendUDP("em", IP_Other, other_port);
//        lastMessageSent = millis();
//        }
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
          sendUDP("em", IP_Other, other_port);
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

//      
//      if(millis()-lastMessageSent > messageFrequency)
//      {
//        
//        sendUDP("oc", IP_Other, other_port);
//        lastMessageSent = millis();
//        }

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

      if (otherOccupied)
      {
        //////////////////////STATE CHANGE////////////////////////////
        if (DEBUG) Serial.println("switching state to Grow");
        if (WDEBUG) Console.println("switching state to Grow");
        currentState = grow;
        sendUDP("oc", IP_Other, other_port);
        growingCounter = NUM_ALTARLEDS;
      }
      if (!presence)
      {
        //////////////////////STATE CHANGE////////////////////////////
        if (DEBUG) Serial.println("switching state to Emptying");
        if (WDEBUG) Console.println("switching state to Emptying");
        currentState = emptying;
        sendUDP("em", IP_Other, other_port);
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
          sendUDP("oc", IP_Other, other_port);
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
      if (!presence || !otherOccupied)
      {
        //////////////////////STATE CHANGE////////////////////////////
        if (DEBUG) Serial.println("switching state to Emptying");
        if (WDEBUG) Console.println("switching state to Emptying");
        currentState = emptying;
        sendUDP("em", IP_Other, other_port);
        emptyingCounter = growingCounter;
      }
      break;


    ///////////////////////////////////////////////////
    //                    BURST                      //
    ///////////////////////////////////////////////////
    case burst:
      
      if (MASTER)
      {
        sendUDP("1", IP_PC, PC_port);
      }
      delay(1000);
      // fade out all leds.
      if (DEBUG) Serial.println("just bursted");
//
//      
      for (int c = 0; c < 50; c++)
      {
        // cool down
        for (int l = 0; l < NUM_LEDS; l++)
        {
          leds[l].fadeToBlackBy(16);
        }
        FastLED.show();
        delay(50);
      }

      if (MASTER) sendUDP("0", IP_PC, PC_port);


      /////////////STATE CHANGE//////////
      if (WDEBUG) Console.println("switching state to Resting");
      if (DEBUG) Serial.println("switching state to REsting");
      restEnter = millis();
      sendUDP("em", IP_Other, other_port);
      currentState = resting;
      // prepare for slow sleeping
      for (int l = 0; l < NUM_ALTARLEDS; l++)
      {
        ledBrightness[l] =  0;
        leds[l] = CHSV(rootHue, 255, ledBrightness[l]);
        fadeAmount[l] = 1;
      }
      break;


    ///////////////////////////////////////////////////
    //                    RESTING                      //
    ///////////////////////////////////////////////////
    case resting:
    // is tehre lights? like charging?
//          if(millis()-lastMessageSent > messageFrequency)
//      {
        //if(DEBUG) Serial.print("sending message");
        //sendUDP("em", IP_Other, other_port);
//        lastMessageSent = millis();
//        }
    if( (millis()-restEnter) > restingTime)
    {
      /////////////STATE CHANGE//////////
      if (WDEBUG) Console.println("switching state to reset");
      if (DEBUG) Serial.println("switching state to Reset");
      currentState = reset;
      sendUDP("em", IP_Other, other_port);
      
      }
    // 
    
    break;
    



    

    default:
      ;
  }

  //if(DEBUG) Serial.print(" 7: ");
    //  if(DEBUG) Serial.print(millis()); 
  FastLED.show();
    //if(DEBUG) Serial.print(" 8: ");
      //if(DEBUG) Serial.println(millis()); 
}
