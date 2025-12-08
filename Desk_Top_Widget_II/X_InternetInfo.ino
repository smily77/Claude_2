void catchTimes() {
  
  utcOffset[0] = getUTCbasedonLocation(location[0]);
  if (firstRun) {
    tft.print("To UTC @ Origin: ");
    tft.println(utcOffset[0]/3600);
  }
  for (int i=1; i <7 ; i++) {
    utcOffset[i] = getUTCbasedonLocation(location[i]);
    if (firstRun) {
      tft.print(airCode[i]);
      tft.print(": ");
      tft.println(utcOffset[i]/3600);
    }
  }
}

long getUTCbasedonLocation(String place) {

  while (!catchWeatherInfo(place, temp, humidity, icon, iconSub, lat, lon));
  if (DEBUG) {
    Serial.println();
    Serial.println("Got weather info");
  }
  while (!catchTZInfo(lon, lat, utc));
  if (DEBUG) {
    Serial.println();
    Serial.println("TZInfo");
  }

  return int(utc*3600);
}

boolean catchWeatherInfo(String city, byte &temp, byte &humidity, byte &icon, char &iconSub, float &lat, float &lon) {
  boolean ok = false;
  
  if (DEBUG) Serial.println("Connect openWeatherInfo");
  
  if (!client.connect("api.openweathermap.org", 80)) {
    if (DEBUG) Serial.println("connection failed");
    return false;
  }

  if (DEBUG) Serial.println("Got Connection to openWeartherInfo");
  
  String url ="/data/2.5/weather?q=" + city + "&units=metric&APPID=" + OPWMapi;

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + "api.openweathermap.org" + "\r\n" + 
               "Connection: close\r\n\r\n");
  
  if (client.connected()) {
    while(!client.available());
    ok = true;
    // lon
    finder.find("coord");
    finder.find("lon");
    byte l = client.read(); //"
    l = client.read(); //:
    lon = finder.getFloat();
    // lat
    finder.find("lat");
    l = client.read(); //"
    l = client.read(); //:
    lat = finder.getFloat();    
    // icon
    finder.find("icon");
    l = client.read(); //"
    l = client.read(); //:
    l = client.read(); //"
    icon = 0;
    icon = int(client.read())-48;
    icon = 10 * icon + (int(client.read())-48);
    iconSub = client.read();    
    // temperature
    finder.find("temp");
    l = client.read(); //"
    l = client.read(); //:
    float temp1 = finder.getFloat();
    temp = (int)temp1;
    if ((temp1 - temp) > 0.5) temp++;
    // humidity
    finder.find("humidity");
    l = client.read(); //"
    l = client.read(); //:
    humidity = finder.getValue();
    
    while(client.available()){
      int c = int(client.read());
      if (DEBUG) Serial.print(char(c));
    }
  }
  if (DEBUG) Serial.println();
  client.stop();
  client.flush();
  return ok;
}

boolean catchTZInfo(float lon, float lat, float &utc) {
  boolean ok = false;
  
  if (!client.connect("api.timezonedb.com", 80)) {
    if (DEBUG) Serial.println("connection failed");
    return false;
  }
//http://api.timezonedb.com/v2/get-time-zone?key=P9YQT3PQ42TB&format=xml&by=position&lat=13.74&lng=-89.21
  //String url = "/v2/get-time-zone?key=P9YQT3PQ42TB&format=xml&by=position&lat="; 
  String url = "/v2/get-time-zone?key="; 
  url += TZapi;
  url += "&format=xml&by=position&lat=";
  url += lat;
  url += "&lng=";
  url += lon;

  if (DEBUG) {
    Serial.println();
    Serial.println(url);
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + "api.timezonedb.com" + "\r\n" + 
               "Connection: close\r\n\r\n");
   if (DEBUG) Serial.println("Sent TZ request");
   if (client.connected()) {
    while(!client.available());
    ok = true;
    if (DEBUG) Serial.println("Got TZ connected");
    finder.find("<gmtOffset>");
    utc = finder.getValue();
    utc = utc / 3600;
    Serial.println();
    Serial.print(utc);
    Serial.println();
    while(client.available()){
      int c = int(client.read());
      if (DEBUG) Serial.print(char(c));
    }
  }
  if (DEBUG) Serial.println();
  client.stop();
  client.flush();
  delay(1000);
  return ok;
}

void catchCurrencies() {

  char* host = "api.frankfurter.app";
  int httpsPort = 443;
  String result;

  if (DEBUG) Serial.println(fxSym[0]);
  if (DEBUG) Serial.println(fxSym[1]);
  if (DEBUG) Serial.println(fxSym[2]);
  if (DEBUG) Serial.println(fxSym[3]);
  
  if (DEBUG) Serial.print("connecting to ");
  if (DEBUG) Serial.println(host);

  clientSec.setInsecure();
    if (!clientSec.connect(host, httpsPort)) {
    if (DEBUG) Serial.println("connection failed");
    return;
  }
  
  String url = "/latest";
  if (DEBUG) Serial.print("requesting URL: ");
  if (DEBUG) Serial.println(url);

  clientSec.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  if (DEBUG) Serial.println("request sent");
  while (clientSec.connected()) {
    String line = clientSec.readStringUntil('\n');
    if (line == "\r") {
      if (DEBUG) Serial.println("headers received");
      break;
    }
  }
  String line = clientSec.readStringUntil('\n');
  if (DEBUG) Serial.println("reply was:");
  if (DEBUG) Serial.println("==========");
  if (DEBUG) Serial.println(line);
  if (DEBUG) Serial.println("==========");
  if (DEBUG) Serial.println("closing connection");

  for (int i=0; i < 4; i++) {
    if(fxSym[i].compareTo("EUR")) {
      result = line.substring(line.indexOf(fxSym[i])+5,line.indexOf(",",line.indexOf(fxSym[i])));
      fxValue[i]= result.toFloat();
    }
    else fxValue[i]=1.00;
    if (DEBUG) Serial.print(fxSym[i]);
    if (DEBUG) Serial.print(":   ");
    if (DEBUG) Serial.println(fxValue[i]);
  }

  for (int l = 1; l < 4 ; l++) {
    fxValue[l] = fxValue[0] / fxValue[l];
    if (firstRun) {
      tft.print(fxSym[l]);
      tft.print(": ");
      tft.println(fxValue[l]);
    }
  }
}
    
  
