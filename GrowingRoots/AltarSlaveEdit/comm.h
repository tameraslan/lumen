#ifndef COMM_H
#define COMM_H




/*** Connection ***/
#include <Process.h>
#include <BridgeUdp.h>
BridgeUDP Udp;
// messaging:
char packetBuffer[255]; //buffer to hold incoming packet

// I am the Slave, my ip: 10,0,0,21, local-port:4444.
int local_port;

// Masters IP: 10.0.0.20, port:3333
IPAddress IP_Other;
int other_port; 
// PC: 
IPAddress IP_PC(10, 0, 0, 10); // PC IP
int PC_port = 7000;

void setup(bool isMaster)
{
  if (MASTER)
  {
    // we are master
    local_port =  4444;
     IP_Other = IPAddress(10, 0, 0, 20);
    other_port = 3333;  
    }
    else
    {
      // we are slave
      IP_Other = IPAddress(10, 0, 0, 21);
      local_port =  3333;
      other_port = 4444;
      }
  }

bool masterOccupiedMessage = false;

bool isMasterOccupied()
{
  return masterOccupiedMessage;
  }
  
void readUdp()
{
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
//    if (DEBUG)
//    {
//      
//      Serial.print("Received packet of size ");
//      Serial.println(packetSize);
//      Serial.print("From ");
//      IPAddress remoteIp = Udp.remoteIP();
//      Serial.print(remoteIp);
//      Serial.print(", port ");
//      Serial.println(Udp.remotePort());
//    }
    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    String phrase;
    phrase = String(phrase + packetBuffer);
    if (len > 0) packetBuffer[len] = 0;
//    if (DEBUG)
//    {
//      Serial.println("Contents:");
//      Serial.println(packetBuffer);
//      Serial.println(phrase);
//    }
    if (phrase == "oc")
    {
      masterOccupiedMessage = true;
      // Serial.println("OtherOccupied == TRUE");
      return;
    }
    else if (phrase == "em")
    {
      masterOccupiedMessage = false;
      // Serial.println("OtherOccupied == FALSE");
      return;
    }
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
