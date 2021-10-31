#include <FS.h>                   //this needs to be first, or it all crashes and burns...

/* =============================================
 *      IV-18 Vaccuum Fluorescent Display clock
 * =============================================
 * 
 * se connecte sur NTP pour lire l'heure UTC
 * utilise la lib TimeZone pour gerer heure ete heure hiver automatiquement (nécessite le fichier TimeZone.ino dans le même dossier)
 * 
 * pilote le driver MAX6921
 * 
 * la conf du wifi est semi-automatique, se connecter sur le serveur depuis un téléphone :
 * connect to 192.168.4.1 to access the configuration web page
 * then select the "CONFIGURE WIFI" tab,
 * then click on your wifi SSID and add the password then save
 * 
 * a RCWL-0516 radar is used to detect the presence and switch ON/OFF, through a MOSFET the tube, the MAX driver and the bulk converter accordingly
 * 
 */
 
#include <ESP8266WiFi.h>
#include <TimeLib.h> 

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <WiFiUdp.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson


//define your default values here, if there are different values in config.json, they are overwritten.
char server_SSID[40] = "";
char server_pswd[20] = "";
char  brightness_cfg[5] = "50";

//flag for saving data
bool shouldSaveConfig = false;

ESP8266WebServer server(80);
String hello = " Welcome to the IV18 page";
uint32_t brightness = 50;
 
//String myIP = "";

//MAX6921 pins
#define PIN_LE    12  // D6 Shift Register Latch Enable
#define PIN_CLK   13  // D7 Shift Register Clock
#define PIN_DATA  14  // D5 Shift Register Data
#define PIN_BL    15  // D8 Shift Register Blank (1=display off     0=display on)

#define PIN_RADAR 16   //output of RCWL-0516 radar motion detector (input)

#define PIN_PWR   2   //(output) 5V power switch connected to a mosfet gate to switch ON/OFF the filament, the bulk converter, the MAX6921


bool show24hr = true; 
bool leadingZero = false;   // whether or not to show the leading zero when in 12 hour mode (e.g. 7am would either show as 7 or 07)

int multiplex_counter = 0 ; //IV18 index of digit to display



// NTP Servers:
/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
IPAddress timeServerIP; // 1.pool.ntp.org NTP server address
const char* ntpServerName = "1.pool.ntp.org" ; // mettre ici le nom du pool NTP

time_t madate;  
time_t monT0; // used to store the last "nobody" time     

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets


// lookup tables for the high and low value of mins/sec to determine
// which number needs to be displayed on each of the minutes and seconds tubes
// e.g. if the clock is to diplay 26 minutes then it 
// will look up the values at the 26th position and display a 2 on the high mins tube and a 6 on the low mins tube
const byte minsec_high[]= {0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,11};
const byte minsec_low[] = {0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,11};

// lookup tables for the high and low value of hours to determine
// which number needs to be displayed on each of the tubes for the hour display
// it can handle if its 24hr display or not and also if a leading zeo is displayed
// the "10" values are to indicate if the leading zero is blanked or not since its outside the range of a normal digit of 0-9
const byte hour_high[2][2][24] = {{{1,10,10,10,10,10,10,10,10,10,1,1,1,10,10,10,10,10,10,10,10,10,1,1},{1,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,1,1}},{{0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2},{0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2}}};
const byte hour_low[2][2][24]  = {{{2, 1, 2, 3, 4, 5, 6, 7, 8, 9,0,1,2, 1, 2, 3, 4, 5, 6, 7, 8, 9,0,1},{2,1,2,3,4,5,6,7,8,9,0,1,2,1,2,3,4,5,6,7,8,9,0,1}},{{0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3},{0,1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6,7,8,9,0,1,2,3}}};

// shift register positions for each digit IV18 tube (10 first bits skipped and used by IV18 segments)
//       Digit  0   1   2   3   4   5   6   7   8   9
//       Outx       18  11  17  12  16  13  14  15  19   (OUT10 non cablé)
//   i 2^(i+10)     8   1   7   2   6   3   4   5   9
int HVOUT[] = { 0,  128,1,  64, 2,  32, 4,  8,  16, 256}; //10 NU

//HVOUT bit value for each segment (
#define segA 0b00000001 //OUT0
#define segF 0b00000010 //OUT1
#define segB 0b00000100 //OUT2
#define segG 0b00001000 //OUT3
#define segE 0b00010000 //OUT4
#define segC 0b00100000 //OUT5
#define segD 0b01000000 //OUT6
#define segH 0b10000000 //OUT7  , 8 et 9 NU



/*----------------------------------------------------------------------*
 * Inlined modified Arduino Timezone Library v1.0                       *                *
 * Jack Christensen Mar 2012                                            *                              *
 *----------------------------------------------------------------------*/

// inlined Timezone.h
//convenient constants for dstRules
enum week_t {Last, First, Second, Third, Fourth}; 
enum dow_t {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};

//structure to describe rules for when daylight/summer time begins,
//or when standard time begins.
struct TimeChangeRule
{
    char abbrev[6];    //five chars max
    uint8_t week;      //First, Second, Third, Fourth, or Last week of the month
    uint8_t dow;       //day of week, 1=Sun, 2=Mon, ... 7=Sat
    uint8_t month;     //1=Jan, 2=Feb, ... 12=Dec
    uint8_t hour;      //0-23
    int offset;        //offset from UTC in minutes
};
        
class Timezone
{
    public:
        Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart);
        Timezone(int address);
        time_t toLocal(time_t utc);
        time_t toLocal(time_t utc, TimeChangeRule **tcr);
        time_t toUTC(time_t local);
        boolean utcIsDST(time_t utc);
        boolean locIsDST(time_t local);
        void readRules(int address);
        void writeRules(int address);

    private:
        void calcTimeChanges(int yr);
        time_t toTime_t(TimeChangeRule r, int yr);
        TimeChangeRule _dst;    //rule for start of dst or summer time for any year
        TimeChangeRule _std;    //rule for start of standard time for any year
        time_t _dstUTC;         //dst start for given/current year, given in UTC
        time_t _stdUTC;         //std time start for given/current year, given in UTC
        time_t _dstLoc;         //dst start for given/current year, given in local time
        time_t _stdLoc;         //std time start for given/current year, given in local time
};


/* how to define a time zone : Define a TimeChangeRule as follows:
 * ==============================================================

TimeChangeRule myRule = {abbrev, week, dow, month, hour, offset};

Where:
abbrev is a character string abbreviation for the time zone; it must be no longer than five characters.
week is the week of the month that the rule starts.
dow is the day of the week that the rule starts.
hour is the hour in local time that the rule starts (0-23).
offset is the UTC offset in minutes for the time zone being defined.

For convenience, the following symbolic names can be used:
week: First, Second, Third, Fourth, Last
dow: Sun, Mon, Tue, Wed, Thu, Fri, Sat
month: Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec

For the Eastern US time zone, the TimeChangeRules could be defined as follows:
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours

For a time zone that does not change to daylight/summer time, pass the same rule twice to the constructor, for example:
Timezone usAZ(usMST, usMST);
 */

// comment/uncomment here after the time zone definition (2 lines) adapted to your location  <<<<<<<<<<<<<<<<<<<

//US Eastern Time Zone (New York, Detroit)
//TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
//TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours

//Central European Time Zone (Paris)
//TimeChangeRule myDST = {"CEST", Last, Sun, Mar, 2, 120};    //Daylight summer time = UTC + 2 hours
//TimeChangeRule mySTD = {"CET", Last, Sun, Oct, 2, 60};     //Standard time = UTC + 1 hours

TimeChangeRule myDST = {"UTC", Last, Sun, Mar, 28, 60};    //Daylight summer time = UTC + 1 hours
TimeChangeRule mySTD = {"UTC", Last, Sun, Oct, 31, 0};     //Standard time = UTC + 0 hours
Timezone myTZ(myDST, mySTD);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets



// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

// time management routines
//=========================
time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");

//get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  
//  sendNTPpacket(timeServer);
  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
     // return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
       return secsSince1900 - 2208988800UL ;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

void handleRoot() {
  Serial.println("requete serveur");
  Serial.println(hello);
  // construction de la string a passer au serveur
  hello = "<html>\
  <head>\
    <title>AEROPIC's IV18 CLOCK...</title>\
    <style> p { text-align:center; } </style>\
    <style> h1 { text-align:center; } </style>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
 </head>\
  <body>\
    <h1>" + hello + "</h1>" +\
     "<p> to change the percentage of brightness to xx percent send the request : </p>"+\
      "<p> IV18_CLOCK.local?B=xx </p>"+\
      "<p>  </p>"+\
     "<p> 0 = OFF --xx-- 100 = MAX </p>"+\
     "<p>  </p>"+\
    "<p> luminosity " + server.arg("B").toInt() +"</p>"+\
    "<p>  </p>"+\
    "<p>" + day() + "/" + month() + "/" + year() + "  " + hour() + ":" + minute() +":" + second() +"</p>"+\
    "<p>" + "ESP8266 RSSI = " + String(WiFi.RSSI())+"</p>" + "</body></html>";
    
  server.send(200, "text/html", hello);

// kill wifi with B=9999
if (server.arg("B").toInt()== 9999) {
  //disconnect WiFi as it's no longer needed
  server.stop(); //indispensable
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("WIFI OFF");
}
else { //compute the brightness value for the PWM
 brightness = 1023-((1023*server.arg("B").toInt())/100);
  Serial.println(server.arg("B").toInt());

  //===========
//save the custom parameters to FS
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["server_SSID"] = WiFi.SSID();
    json["server_pswd"] = WiFi.psk();
    json["brightness"]= String(brightness);
    Serial.println( WiFi.SSID() );
   // Serial.println( WiFi.psk() );

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
// ===========
  
}
  
  hello = "Welcome to the IV18 page"; // reset the hello string
}

void handleNotFound() {
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}


// setup
// ======
void setup() {

  pinMode(PIN_LE,  OUTPUT);
  pinMode(PIN_BL,  OUTPUT); // a priori inutile avec le PWM
  pinMode(PIN_DATA,OUTPUT);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_RADAR, INPUT);
  pinMode(PIN_PWR, OUTPUT);
  digitalWrite(PIN_PWR, HIGH); // switch ON power
 
  write_vfd_iv18(9,128); //init driver : disolay the left big dot while  wifi is not connected
  
 // start the derial port
  Serial.begin(115200);
  Serial.println("Connecting to ");
  //Serial.println(ssid);

// manage config file
// ==================
 //clean FS, for testing (usefull for first boot to be sure the FS is formatted)
 // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(server_SSID, json["server_SSID"]);
          strcpy(server_pswd, json["server_pswd"]);
         strcpy(brightness_cfg, json["brightness"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  Serial.println(server_SSID);
  Serial.println(String(server_pswd));

  if (String(server_pswd) == "") {  //password pas renseigné
    Serial.println("server_pswd pas renseigne");
     //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset saved settings
  //  wifiManager.resetSettings();

    wifiManager.setConfigPortalTimeout(180); //time out 2 minutes
    
    //set custom ip for portal
    // wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration (with time out)
    // connect to 192.168.4.1 to access the configuration web page
    wifiManager.autoConnect("AutoConnectAP");
    Serial.println( WiFi.status() ); // on pourrait tester le status 0 en cas de sortir par timeout et non parametré
    //or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
    //if you get here you have connected to the WiFi
Serial.println("connected...yeey :)");


  //save the custom parameters to FS
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["server_SSID"] = WiFi.SSID();
    json["server_pswd"] = WiFi.psk();
    json["brightness"]= "50";
    Serial.println( WiFi.SSID() );
    Serial.println( WiFi.psk() );

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  //end save
  } // end if password pas renseigné

  else {
  //   wifi stuff
  //   ==========
  // init of wifi with  box
  WiFi.disconnect();
  WiFi.begin(server_SSID, server_pswd);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  }
 

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("IV18_CLOCK")) {
    Serial.println("MDNS responder started");
  }

//init and get the time
  Serial.println("get TimeNTP ...");
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(90000);         // set the number of seconds between re-sync: here once a day

  madate =  myTZ.toLocal(now(), &tcr);
  Serial.println(madate);
  monT0 = madate; // init the last "nobody" time

  // manage http server
  // ==================
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  // uncomment to enable server
  // ===============
  //server.begin(); 
  //Serial.println("HTTP server started");

  // retrieve the brightness from FS
  brightness = (String(brightness_cfg)).toInt();
  Serial.println(brightness);

  //disconnect WiFi to save power
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.println("Setup complete WIFI disabled");
} // end init


// =====================================
//             main loop               
// =====================================
void loop() {

 // trim the tube brightness
 analogWrite(PIN_BL,brightness);

 //set the time
 madate =  myTZ.toLocal(now(), &tcr);
 
// manage the radar
if (digitalRead(PIN_RADAR)) {
  digitalWrite(PIN_PWR, HIGH); // switch ON power
  monT0 = madate;
}
else 
{
  if ((madate - monT0)>= 300){
    digitalWrite(PIN_PWR, LOW); // switch Off power after 300 sec with no motion
  }
}
//IV18 stuff
    multiplex_counter++; 
    if (multiplex_counter > 8)  multiplex_counter = 1;  //multiplex on the 9 digits (skip digit 9)
    char d;
 
    switch (multiplex_counter) 
    {
      case(9): 
      {
        d = '!' ;
        break;
      }
      case(8): 
      {
        d = hour_high[show24hr][leadingZero][hour(madate)];
       //  d = '2';
        break;
      }
      case(7): 
      {
        d = hour_low[show24hr][leadingZero][hour(madate)];
     //    d = '3';
        break;
      }
      case(6): 
      {
        d = '-';
       //  d = '4';
        break;
      }    
      case(5): 
      {
        d = minsec_high[minute(madate)];
  //    d = '5';
        break;
      }
      case(4): 
      {
        d = minsec_low[minute(madate)];
   //   d = '6';
        break;
      }
      case(3): 
      {
        d = '-';
        break;
      }  
      case(2): 
      {
        d = minsec_high[second(madate)];
 //     d = '3';
        break;
      }
      case(1): 
      {
        d = minsec_low[second(madate)];
 //     d = '2';
        break;
      }    
    } 
    write_vfd_iv18(multiplex_counter, calculate_segments_7(d));
     delayMicroseconds(3); //usefull to reduce flicker

    //reboot @3 o'clock
    if ((hour(madate) == 03) && (minute() ==0) && (second() ==0)) ESP.restart();

    // handle the web server clients
    server.handleClient(); 
}// end loop



// definition du tableau segments numeros
char segments_7n[] = {
  segA + segB + segC + segD + segE + segF ,  //0
  segB + segC, //1
  segA + segB + segD + segE + segG, //2
  segA + segB + segC + segD + segG, //3
  segB + segC + segF + segG, //4
  segA + segC + segD + segF + segG, //5 S
  segA + segC + segD + segE + segF + segG, //6
  segA + segB + segC, //7
  segA + segB + segC + segD + segE + segF + segG, //8
  segA + segB + segC + segD + segF + segG, //9
  };

  // definition du tableau segments ASCII A-Z 
char segments_7a[] = {
  segA + segB + segC + segE + segF + segG, //10 A
  segC + segD + segE + segF + segG, //11 B
  segA + segD + segE + segF, //12 C
  segB + segC + segD + segE + segG, //13 D
  segA + segD + segE + segF + segG, //14 E
  segA + segE + segF + segG, //15 F
  segA + segC + segD + segE + segF, //G
  segB + segC + segE + segF + segG, //H
  segB + segC, // I
  segB + segC + segD + segE, //J
  segB + segE + segF + segG, //K
  segD + segE + segF, //L
  segA + segC + segE + segG, //M
  segC + segE + segG, //N
  segA + segB + segC + segD + segE + segF,  //0
  segA + segB + segE + segF + segG, //P
  segA + segB + segC + segF + segG, //Q
  segE + segG, //R
  segA + segC + segD + segF + segG, //5 S
  segD + segE + segF + segG, //T
  segB + segC + segD + segE + segF, //U
  segC + segD + segE, //V
  segA + segD + segG, //W
  segA + segD + segG, //X
  segB + segC + segD + segF + segG, //Y
  segD + segG//Z
};


// calculate  segments acording their  ASCII code
uint8_t calculate_segments_7(uint8_t ch)
{
  uint8_t segs = 0;
  if ((ch >= 0) && (ch <= 9))           //digits
    segs = segments_7n[ch];
  else if ((ch >= '0') && (ch <= '9'))  //digits in Ascii
    segs = segments_7n[ch-48];
  else if ((ch >= 'A') && (ch <= 'Z'))  // A-Z
    segs = segments_7a[ch-'A'];
  else if ((ch >= 'a') && (ch <= 'z'))  // a-z
    segs = segments_7a[ch-'a'];
  else if (ch == '-')
    segs = segG;
  else if (ch == '"')
    segs = segB+segF;
  else if (ch == 0x27)  // "'"
    segs = segB;
  else if (ch == '_')
    segs = segD;
    else if (ch == '*')
    segs = segH;
    else if (ch == '!')
    segs = segH+segB;
  else
    segs = 0;
  return segs;
} // end calculate_segments_7

// Write 8 bits to HV5812 driver
void write_vfd_8bit(uint8_t data)
{
  // shift out MSB first
  for (uint8_t i = 0; i < 8; i++)  {
    if (!!(data & (1 << (7 - i))))
      digitalWrite(PIN_DATA,HIGH);
    else
      digitalWrite(PIN_DATA,LOW);

    digitalWrite(PIN_CLK,HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_CLK,LOW);
  }
} // end write_vfd_8bit

// Writes to the HV5812 driver for IV-18
void write_vfd_iv18(uint8_t digit, uint8_t segments)
{
 // segments = 255; // pour test affichage de tous les segments
  uint32_t val = ((HVOUT[digit])<<11) | ((uint32_t)segments );
//Serial.println(val);
  write_vfd_8bit(0); // unused upper byte
  write_vfd_8bit(val >> 16);
  write_vfd_8bit(val >> 8);
  write_vfd_8bit(val); 
   
  digitalWrite(PIN_LE,HIGH); //latch the value
  delayMicroseconds(1);
  digitalWrite(PIN_LE,LOW);
} // end write_vfd_iv18
