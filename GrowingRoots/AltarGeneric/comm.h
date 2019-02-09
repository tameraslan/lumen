#ifndef COMM_H
#define COMM_H




/*** Connection ***/
#include <Process.h>
#include <BridgeUdp.h>
BridgeUDP Udp;
// messaging:
char packetBuffer[255]; //buffer to hold incoming packet

// if Slave: my ip: 10,0,0,21, local-port:4444.
// if Master: my ip: 10,0,0,20 local-Port: 3333
int local_port;

// Masters IP: 10.0.0.20, port:3333
IPAddress IP_Other;
IPAddress Master(10, 0, 0, 20);
IPAddress Slave(10, 0, 0, 21);

int other_port; 
// PC: 
IPAddress IP_PC(10, 0, 0, 10); // PC IP
int PC_port = 7000;

int getLocalPort()
{
  return local_port;
  }
int getOtherPort()
{
  return other_port;
  }

IPAddress getOtherIp()
{
  return IP_Other;
  }

  int getPCPort()
{
  return PC_port;
  }

IPAddress getPCIp()
{
  return IP_PC;
  }
  
void setComms(bool isMaster)
{
  if(DEBUG) Serial.println("setting up comm");
  if (isMaster)
  {
    if(DEBUG) Serial.println("we are master");
    // we are master
    local_port =  3333;
     IP_Other = Slave;
    other_port = 4444;  
    }
    else
    {
      // we are slave
      if(DEBUG) Serial.println("we are slave");
      IP_Other = Master; //.21
      local_port =  4444;
      other_port = 3333;
      }
  }

bool otherOccupiedMessage = false;

bool isOtherOccupied()
{
  return otherOccupiedMessage;
  }
  
bool readUdp()
{
  
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    //if(DEBUG) Serial.print("got message");
    
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
    if (phrase == "oc")
    {
      otherOccupiedMessage = true;
      // Serial.println("OtherOccupied == TRUE");
      return 1;
    }
    else if (phrase == "em")
    {
      otherOccupiedMessage = false;
      // Serial.println("OtherOccupied == FALSE");
      return 1;
    }
    else 
    {
      return 0;
      //otherOccupiedMessage = false;
    }
    
  }
  else 
  {
    return 0;
    //otherOccupiedMessage = false;
  }
  
  //return false;
  //masterOccupiedMessage = false;
  //return;
}


void sendUDP(uint8_t data[], IPAddress IP, int port)
{
  Udp.beginPacket(IP, port);
  Udp.write(data, sizeof(data));
  Udp.endPacket();
}




#endif
