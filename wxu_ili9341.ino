#include <SPI.h>
#include <Wire.h>
#include "Adafruit_WINC1500.h"
#include "Adafruit_WINC1500Udp.h"
#include "ArduinoJson.h"
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "TimeLib.h"

Adafruit_AlphaNum4 alphaGRN = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 alphaWHT = Adafruit_AlphaNum4();
Adafruit_AlphaNum4 alphaYEL = Adafruit_AlphaNum4();

#define WINC_CS  8
#define WINC_IRQ 7
#define WINC_RST 4
#define WINC_EN  2
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);
Adafruit_WINC1500Client client;
Adafruit_WINC1500UDP Udp;

#define TFT_CS    11   // grn 
#define TFT_RST   10   // org
#define TFT_DC    9    // wht
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
#define BLACK     0x0000
#define BLUE      0x001F
#define RED       0xF800
#define GREEN     0x07E0
#define CYAN      0x07FF
#define MAGENTA   0xF81F
#define YELLOW    0xFFE0 
#define WHITE     0xFFFF

int status = WL_IDLE_STATUS;
char ssid[] = "pizgloria";
char pass[] = "HomeCall4";

unsigned int localPort = 8888;
// unsigned int localPort = 2390;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[ NTP_PACKET_SIZE ];
IPAddress timeServer(129, 6, 15, 28);
// IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov

const int timeZone = -5;

#define WUhost "api.wunderground.com"
#define WUkey  "712a91952a8b182e"
#define WUloc  "PDK"

char LastForecastFetch[30];
char LastConditionsFetch[30];
char ForecastDay0[30];
char tempStr[30];
char LtempStr[30];
char presStr[30];
char humStr[30];
char windStr[30];
char condStr[30];
char tStamp[10];
char NextFetch[20];
int fetchInt = 600;

void setup() {
#ifdef WINC_EN
    pinMode(WINC_EN, OUTPUT);
    digitalWrite(WINC_EN, HIGH);
#endif
    Serial.begin(115200);
    Wire.begin();
    tft.begin();
    tft.fillScreen(BLUE);
    tft.setRotation(2);
    tft.setTextSize(2);
    tft.setTextColor(WHITE, BLUE);
//    while (!Serial) { ; }
    while (status != WL_CONNECTED) { status = WiFi.begin(ssid, pass); }
    tft.setCursor(0,0); tft.print(">"); tft.print(ssid);
    for (int i=9; i > 0; i--){ ; tft.print(">"); delay(1000); }
    // printWifiStatus();
    alphaGRN.begin(0x73);
    alphaWHT.begin(0x74);
    alphaYEL.begin(0x76);
    Udp.begin(localPort);
    setSyncProvider(getNtpTime);
    delay(2000);

    sprintf(tStamp, "%02d:%02d", hour(),minute());
    tft.setCursor(20,30);
    tft.print("Start: "); tft.println(tStamp);
    tft.drawLine(20, 70, 220, 70, WHITE);
}

static char respBuf[8192];

void loop() {    
    fetchWU("forecast");
    delay(5000);
    fetchWU("conditions");
    tft.setTextColor(YELLOW, BLUE);
    for (int i = fetchInt ; i > 0 ; i--){ 
        sprintf(NextFetch, "Next in %03d Seconds", i);
        tft.setCursor(0,295);
        tft.print(NextFetch); 
        delay(1000);
     }
}

void fetchWU(char *WUtype) {
    client.stop();
    delay(100);
    if (!client.connect(WUhost, 80)) { return; }
    client.print("GET /api/");
    client.print(WUkey);
    client.print("/");
    client.print(WUtype);
    client.print("/q/");
    client.print(WUloc);
    client.println(".json HTTP/1.1");
    client.print("Host: "); 
    client.println(WUhost);
    client.println("Connection: close");
    client.println();
    client.flush();
    int respLen = 0;
    bool skip_headers = true;
    while (client.connected() || client.available()) {
        if (skip_headers) { 
            String aLine = client.readStringUntil('\n'); 
            if (aLine.length() <= 1) { skip_headers = false; } 
        }
        else {
            int bytesIn;
            bytesIn = client.read((uint8_t *)&respBuf[respLen], sizeof(respBuf) - respLen);
            if (bytesIn > 0) {  respLen += bytesIn; if (respLen > sizeof(respBuf)) respLen = sizeof(respBuf); }
            else if (bytesIn < 0) { }
        }
        delay(1);
    }
    client.stop();
    if (respLen >= sizeof(respBuf)) { return; }
    respBuf[respLen++] = '\0';
    if (WUtype == "conditions") {
        parseConditions(respBuf);
        if (true) { return; }
        if (false) { tft.println("Parse fail -conditions > \r\n"); return; }
    }
    if (WUtype == "forecast") {
        parseForecast(respBuf);
        if (true) { return; }
        if (false) { tft.println("Parse fail -forecast> \r\n"); return; }
    }
    
    return; 
}

bool parseForecast(char *json) {
    StaticJsonBuffer<8192> jsonBuffer;
    char *jsonstart = strchr(json, '{');
    
    if (jsonstart == NULL) { return false; }
    
    json = jsonstart;
    JsonObject& root = jsonBuffer.parseObject(json);
    JsonObject& current = root["forecast"]["simpleforecast"];

    if (!root.success()) { return false; }

    int DateDa = current["forecastday"][0]["date"]["day"];
    int DateMo = current["forecastday"][0]["date"]["month"];
    int DateYr = current["forecastday"][0]["date"]["year"];
    int Htemp =  current["forecastday"][0]["high"]["fahrenheit"];
    int Ltemp =  current["forecastday"][0]["low"]["fahrenheit"];

    sprintf(ForecastDay0, "Forecast %02d/%02d/%02d", DateMo, DateDa, DateYr);

    tft.drawLine(20, 70, 220, 70, WHITE);
    tft.setTextColor(WHITE, BLUE);
    tft.setCursor(0,80);
    sprintf(tStamp, "%02d:%02d", hour(),minute());
    tft.print("Forecast @ "); tft.println(tStamp);
    tft.println(ForecastDay0);
    
    alphaWHT.writeDigitAscii(0, ( Htemp / 10 + 48 ) );
    alphaWHT.writeDigitAscii(1, ( Htemp % 10 + 48 ) );
    alphaWHT.writeDigitAscii(2, 'F');
    alphaWHT.writeDigitAscii(3, ' ');
    alphaWHT.writeDisplay();
    alphaGRN.writeDigitAscii(0, (Ltemp / 10 + 48 ) );
    alphaGRN.writeDigitAscii(1, (Ltemp % 10 + 48) );
    alphaGRN.writeDigitAscii(2, 'F');
    alphaGRN.writeDigitAscii(3, ' ');
    alphaGRN.writeDisplay();
       
    return true;
}

bool parseConditions(char *json) {
    StaticJsonBuffer<8192> jsonBuffer;
    char *jsonstart = strchr(json, '{');

    if (jsonstart == NULL) { return false; }
    json = jsonstart;
    JsonObject& root = jsonBuffer.parseObject(json);
    JsonObject& current = root["current_observation"];
    
    if (!root.success()) { return false; }

    double temp =          current["temp_f"];
    const char *cond =     current["weather"];
    int visi =             current["visibility_mi"];
    const char *UV =       current["UV"];
    const char *humi =     current["relative_humidity"];
    const char *pres_in =  current["pressure_in"];
    const char *trend =    current["pressure_trend"];
    double wind_mph =      current["wind_mph"];
    int wind_mphR =        round(wind_mph);
    int wind_head =        current["wind_degrees"];
    const char *wind_dir = current["wind_dir"];

    int tempR = round(temp);
    sprintf(presStr, "Baro %sinHG-%s", pres_in, trend);
    sprintf(condStr, "%s %imi-vis UV-%s", cond, visi, UV);
    sprintf(humStr,  "Humidity-%s", humi);
    sprintf(windStr, "Wind %iMpH-%s", wind_mphR, wind_dir);
    sprintf(tStamp, "%02d:%02d", hour(),minute());

    tft.drawLine(20, 140, 220, 140, WHITE);
    tft.setTextColor(WHITE, BLUE);
    tft.setCursor(0,150); 
    tft.print("Conditions @ "); tft.println(tStamp);
    tft.println(ForecastDay0);
    tft.println(presStr);
    tft.println(humStr);
    tft.println(windStr);
    tft.println(condStr);
    
    alphaYEL.writeDigitAscii( 0, ( tempR / 10 ) + 48 );
    alphaYEL.writeDigitAscii( 1, ( tempR % 10 ) + 48 );
    alphaYEL.writeDigitAscii( 2, 'F');
    alphaYEL.writeDigitAscii( 3, ' ');    
    alphaYEL.writeDisplay();
    return true;
}

void printWifiStatus() {
    tft.print("SSID: "); tft.println(WiFi.SSID());
    IPAddress ip = WiFi.localIP();
    tft.print("IP Address: "); tft.println(ip);
    long rssi = WiFi.RSSI();
    tft.print("signal strength (RSSI):"); tft.print(rssi); tft.println(" dBm"); 
}

time_t prevDisplay = 0;

time_t getNtpTime() {
    while (Udp.parsePacket() > 0);
    sendNTPpacket(timeServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            Udp.read(packetBuffer, NTP_PACKET_SIZE);
            unsigned long secsSince1900;
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
    }
    return 0;
}

void sendNTPpacket(IPAddress &address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0]   = 0b11100011;
    packetBuffer[1]   = 0;
    packetBuffer[2]   = 6;
    packetBuffer[3]   = 0xEC;
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;              
    Udp.beginPacket(address, 123);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}
