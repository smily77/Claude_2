void readBMP(double &T, double &P) {

   status = pressure.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);
    status = pressure.getTemperature(T);
    if (status != 0)
    {
      // Print out the measurement:
      status = pressure.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);
        status = pressure.getPressure(P,T);
        if (status != 0)
        {

        }
      }
    }
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime(){
  time_t tempTime;
  
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
//      tempTime = secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
      tempTime = secsSince1900 - 2208988800UL + utcOffset[0];
      // if (adjustDstEurope(tempTime)) tempTime = tempTime + 3600;
      if (DEBUG) Serial.println(tempTime);
      return tempTime;
    }
  }
  return 0;
}

boolean adjustDstEurope(unsigned long i)
{
 // last sunday of march
 int beginDSTDate=  (31 - (5* year(i) /4 + 4) % 7);
 int beginDSTMonth=3;
 //last sunday of october
 int endDSTDate= (31 - (5 * year(i) /4 + 1) % 7);
 int endDSTMonth=10;
 // DST is valid as:
 if (((month(i) > beginDSTMonth) && (month(i) < endDSTMonth))
     || ((month(i) == beginDSTMonth) && (day(i) >= beginDSTDate)) 
     || ((month(i) == endDSTMonth) && (day(i) <= endDSTDate)))
 return true;  // DST europe = utc +2 hour
 else return false; // nonDST europe = utc +1 hour
}
