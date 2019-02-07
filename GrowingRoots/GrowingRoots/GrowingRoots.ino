// debugging mode or not.
#define DEBUG false

/*** Connection ***/

#include <Process.h>
#include <BridgeUdp.h>

BridgeUDP Udp;

char packetBuffer[255]; //buffer to hold incoming packet

// I'am slave and number 20! Port 3333
IPAddress IP_Arduino(10,0,0,20); // other Arduino IP // Master=21->other 20, Slave=20 ->other 21
IPAddress IP_PC(10,0,0,10); // PC IP

int local_port = 4444; // 4 slave, 3 master
int PC_port = 7000;
int arduino_port = 3333; // 3 slave, 4 master

bool otherOccupied = false; // holds information whether other Arduino is occupied (true) or not (false)

bool filled = false;

/*** LED Control ***/

#include "FastLED.h"
FASTLED_USING_NAMESPACE
#define DATA_PIN    13
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS   17
CRGB leds[NUM_LEDS];
#define BRIGHTNESS     150
#define FRAMES_PER_SECOND  120
#define WHITE CRGB(50, 155, 5);
#define BLACK CRGB(0, 0, 0);

int activeLEDs = 4; // initially 4 Pixel per altar

 unsigned long timer = 0; // hold timing information
 int fadeValue = 150; // initial HSV brightness value

 bool up = false; // pulsing brighter or darker

 bool pulsing = false;

/*** Sensor Control ***/

// how many sensor are connected?
#define NUM_SENSORS 4
 
// filtered sensor data is kept here:
float sensorVoltage[NUM_SENSORS];

bool capture[NUM_SENSORS];
long unsigned int captureTime[NUM_SENSORS];

// Filtering variables (averaging)
  #define NUM_READINGS 16 //32
  int readings[NUM_SENSORS][NUM_READINGS];
  long unsigned int runningTotal[NUM_SENSORS];                  // the running total
  int average[NUM_SENSORS];
  // init of op-var
  int index = 0;                  // the index of the current reading
  unsigned long prevMillis = 0;

  // filtering variables:
  int sensorReadingInterval = 25; //55
  
  int captureDuration = 1000;
  float captureVoltage[NUM_SENSORS];

  float thresholds[] = {0.5, 0.5, 0.5, 0.5, 1.0, 1.0};

/*** State Machine ***/ 
enum state
{
  empty, //noone
  occupied, // enter 
  grow, // both arduinos filled
  degrow, // someone steps out of one altar
  burst, // both altars filled long enough
  rest // pause after burst
};

state currentState = empty; // start with empty state

void setup()
{
    delay(1000); // whait till everything is ready
    if (DEBUG) Serial.begin(115200);

    for (int i = 0; i < NUM_SENSORS; i++)
    {
        captureVoltage[i] = thresholds[i]; // initialize capture values
    }

    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); // initialize leds
    FastLED.setBrightness(BRIGHTNESS); // initialize led brightness

    FastLED.showColor(CRGB(0, 0, 0));  // all LEDs off
    
    Bridge.begin();        // initialize Bridge
    Udp.begin(local_port); // start listening to local port
}

void loop()
{  
  if(readUdp()) otherOccupied = true;
  // control machine
  switch (currentState)
  {
    case empty:
    timer = 0;
    /***
     * LED 0 lights up
     * LED 3 pulses     
     * start capturing sensor data 
     */
      leds[0] = WHITE;
      leds[0].nscale8_video(100);      
      leds[1] = BLACK;
      leds[2] = BLACK;
      leds[3] = WHITE;    

      if(otherOccupied)
      {
          Serial.println("otherOccupied in loop");
          if(fadeValue>=250) up=false;
          if(fadeValue<=100) up=true;
        
          if(up) fadeValue+=3;
          else fadeValue-=3;    
      }
      else
      {
          if(fadeValue>=250) up=false;
          if(fadeValue<=100) up=true;
        
          if(up) fadeValue++;
          else fadeValue--;    
      }
        leds[3].nscale8_video(fadeValue);
      
      if (sensoring()) 
      {
        currentState = occupied;
        Serial.println("Empty: Switch 2 Occupied");
        leds[3] = BLACK;
      }
      break;
    case occupied:    
      leds[0] = WHITE;
      pulsing = true;
      
      if (millis()%10==0) 
      {
        timer++;
      
        if(timer==150)  leds[1] = WHITE; // 15 Sekunden 4
        if(timer==250) leds[2] = WHITE;  // + 13 Sekunden 6
        if(timer==450) // +14
        {
          leds[3] = WHITE;
          sendUDP("oc", IP_Arduino, arduino_port);
          if(DEBUG)  Serial.println("Send occupied");
          filled = true;
        }
  
        if(pulsing)
        {
          if(fadeValue>=250) up=false;
          if(fadeValue<=100) up=true;
        
           if(up) fadeValue+=4;
            else fadeValue-=4;         
        } 
        FastLED.setBrightness(fadeValue);   
      }
      if (!sensoring())
      {
        currentState = empty; 
        filled = false;
        sendUDP("em", IP_Arduino, arduino_port);
        if(DEBUG) Serial.println("Send empty");
        Serial.println("Occupied: Switch 2 Empty");
      }
      else if (otherOccupied && filled) 
      {
        currentState = grow;
        Serial.println("Occupied: Switch 2 Grow");
      }
      break;
   
    case grow:
      timer = 0;
      pulsing = true;
    /**  if (millis()%50==0) timer++;
      
      if(timer == 10 && activeLEDs<NUM_LEDS) activeLEDs++;   
     //if (activeLEDs<NUM_LEDS) activeLEDs++;   
      
      for(int i=4; i<activeLEDs; i++)       {
        leds[activeLEDs]=WHITE;        
      }**/

       if (millis()%300==0 && activeLEDs<NUM_LEDS) activeLEDs++;      
      
      for(int i=0; i<=activeLEDs; i++)
      {
        leds[i]=WHITE;        
      }

        if(pulsing)
        {
          if(fadeValue>=250) up=false;
          if(fadeValue<=100) up=true;
        
           if(up) fadeValue+=4;
            else fadeValue-=4;         
        } 
        FastLED.setBrightness(fadeValue);   
   
      if (!sensoring())
      {
        filled = false;
        sendUDP("em", IP_Arduino, arduino_port);
        if(DEBUG) Serial.println("Send empty");
        currentState = empty;
        Serial.println("Grow: Switch 2 Empty");
      }
      if (!otherOccupied)
      {
        currentState = degrow;
        Serial.println("Grow: Switch 2 Degrow");
      }

      if(activeLEDs == NUM_LEDS)
      {
        currentState = burst; 
        Serial.println("Grow: Switch 2 Burst");
        sendUDP("1", IP_PC, PC_port);
        if(DEBUG) Serial.println("Send burst");       
      }
      
      // both altars start growing at the same time, until all road is taken.
      // if (lampCount reached) currentState = burst.
      // if someone leaves:
      // currentState = degrow;
      // other altar is also occupuied
      // send osc 0
      break;
    case degrow:
      if (millis()%200==0 && activeLEDs>0) activeLEDs--;      
      
      for(int i=NUM_LEDS; i>activeLEDs; i--)
      {
        leds[i]=BLACK;        
      }
      if(activeLEDs==0)
      {
        currentState = empty;
        Serial.println("Degrow: Switch 2 Empty");
      }
      // check all lights turned off, back to enter.
      // currentState = enter;
      // grow vorgang unterbrochen während grow-phase -> jemand geht raus.
      // check all lights turned off, back to enter.
      // currentState = occupied;
      // send osc 0
      break;
  case burst:
      FastLED.setBrightness(255);
      // give it some time until overall burst appears and fadeout finishes.
      // if (millies-burstTime>5 sec) currentState = rest.
      // climax reached, fade out lights, send message to whole
      // grow erfolgreich beendet      
      if (millis()%50==0 && activeLEDs>0) activeLEDs--;      
      
      for(int i=NUM_LEDS; i>0; i--)
      {
        leds[i]=BLACK;        
      }
      if(activeLEDs==0)
      {
        currentState = rest;
        Serial.println("Degrow: Switch 2 Rest");
      }
      break;
  
    case rest:
      // sleep for 5 mins.
      //if (millies-burstTime>300 sec) currentState = noone.
            // sleep for 5 mins.

      /*if (millis()%100==0) timer++;

      if (timer==30)
      {
        currentState = empty;*/
        delay(30000);
        Serial.println("Rest: Switch 2 Empty");
      
      break;
    default:
      break;
  }
        FastLED.show();
}

bool readUdp()
{
 // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    if (DEBUG)
    {
      Serial.print("Received packet of size ");
      Serial.println(packetSize);
      Serial.print("From ");
      IPAddress remoteIp = Udp.remoteIP();
      Serial.print(remoteIp);
      Serial.print(", port ");
      Serial.println(Udp.remotePort());
    }
    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    String phrase;
    phrase = String(phrase + packetBuffer);
    if (len > 0) packetBuffer[len] = 0;
    if (DEBUG)
    {
      Serial.println("Contents:");
      Serial.println(packetBuffer);
      Serial.println(phrase);
    }
    if(phrase == "oc")
    {
      // Serial.println("OtherOccupied == TRUE");
      return true;
    }
    else if (phrase == "em")
    {
      otherOccupied = false;
     // Serial.println("OtherOccupied == FALSE");
      return false;
    }    
  }
  return false;
}

bool sensoring()
{
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
      }
      
    /*  if(DEBUG)
      {
       Serial.print (sensorVoltage[0]);              
       Serial.print("      ");
       Serial.print (sensorVoltage[1]);
       Serial.print("      ");
       Serial.print (sensorVoltage[2]);
       Serial.print("      ");
       Serial.print (sensorVoltage[3]);
       Serial.println("");
      }  */
      prevMillis = millis();
    }
   // SENSOR CAPTURE: IF SENSOR DETECT AN OBJECT, THEY REMAIN TRIGGERED FOR A PREDEFINED TIME
    for (int s = 0; s < NUM_SENSORS; s++)
    {
      if (sensorVoltage[s] > captureVoltage[s])
      {
        capture[s] = true;
        // record time
        captureTime[s] = millis();
      }
      else
      {
        if ((millis() - captureTime[s]) > captureDuration)
        {
          capture[s] = false;
        }
      }
    }
    bool presence = false;
    for (int t = 0; t < NUM_SENSORS; t++)
    {
      presence = presence || capture[t];  
    }
 
    /*if (DEBUG)
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
    }*/
    return presence;
}

 void sendUDP(uint8_t data[], IPAddress IP, int port)
 {
        Udp.beginPacket(IP, port);
        Udp.write(data, sizeof(data));
        Udp.endPacket();
}
