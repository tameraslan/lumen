#ifndef COMM_H
#define COMM_H




/*** Connection ***/
#include <Process.h>
#include <BridgeUdp.h>
BridgeUDP Udp;
// messaging:
char packetBuffer[255]; //buffer to hold incoming packet

// I am the Master, my ip: 10,0,0,20, local-port:3333.
int local_port = 3333; // 4 slave, 3 master
// the Slave, my ip: 10,0,0,21, local-port:4444.
IPAddress IP_Slave(10, 0, 0, 21); 
int slave_port = 4444; // 3 slave, 4 master
IPAddress IP_PC(10, 0, 0, 10); // PC IP
int PC_port = 7000;


bool slaveOccupiedMessage = false; 

bool isSlaveOccupied()
{
  return slaveOccupiedMessage;
  }
  
void readUdp()
{
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
//    if (DEBUG)
//    {
//      Serial.print("Received packet of size ");
//      Serial.println(packetSize);
//      Serial.print("From ");
//      IPAddress remoteIp = Udp.remoteIP();
//      Serial.print(remoteIp);
//      Serial.print(", port ");
//      Serial.println(Udp.remotePort());
//    }
//    if (WDEBUG)
//    {
//      Console.print("Received packet of size ");
//      Console.println(packetSize);
//      Console.print("From ");
//      IPAddress remoteIp = Udp.remoteIP();
//      Console.print(remoteIp);
//      Console.print(", port ");
//      Console.println(Udp.remotePort());
//    }
    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer, 255);
    String phrase;
    phrase = String(phrase + packetBuffer);
    if (len > 0) packetBuffer[len] = 0;
//    if (WDEBUG)
//    {
//      Console.println("Contents:");
//      Console.println(packetBuffer);
//      Console.println(phrase);
//    }
    if (phrase == "oc")
    {
      slaveOccupiedMessage = true;
      // Serial.println("OtherOccupied == TRUE");
      return;
    }
    else if (phrase == "em")
    {
      slaveOccupiedMessage = false;
      // Serial.println("OtherOccupied == FALSE");
      return;
    }
  }
  //return false;
  //slaveOccupiedMessage = false;
  //return;
}


void sendUDP(uint8_t data[], IPAddress IP, int port)
{
  Udp.beginPacket(IP, port);
  Udp.write(data, sizeof(data));
  Udp.endPacket();
}




#endif
