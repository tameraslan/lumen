#ifndef COMM_H
#define COMM_H




/*** Connection ***/
#include <Process.h>
#include <BridgeUdp.h>
BridgeUDP Udp;
// messaging:
char packetBuffer[255]; //buffer to hold incoming packet
// I am the Master, my ip: 10,0,0,21, local-port:4444.
IPAddress IP_Slave(10, 0, 0, 20); // other Arduino IP // Master=21->other 20, Slave=20 ->other 21
IPAddress IP_PC(10, 0, 0, 10); // PC IP
int local_port = 4444; // 4 slave, 3 master
int PC_port = 7000;
int slave_port = 3333; // 3 slave, 4 master


void readUdp()
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
    if (phrase == "oc")
    {
      //slaveOccupied = true;
      // Serial.println("OtherOccupied == TRUE");
      return true;
    }
    else if (phrase == "em")
    {
      //slaveOccupied = false;
      // Serial.println("OtherOccupied == FALSE");
      return false;
    }
  }
  //return false;
}


void sendUDP(uint8_t data[], IPAddress IP, int port)
{
  Udp.beginPacket(IP, port);
  Udp.write(data, sizeof(data));
  Udp.endPacket();
}




#endif
