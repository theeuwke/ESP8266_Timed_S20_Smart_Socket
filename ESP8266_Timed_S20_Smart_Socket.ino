//OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//WiFiManager
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>   

//Blynk
#define BLYNK_PRINT Serial

//Hardware
#include <TimedBlink.h>

//Time
#include <time.h>
#include <NtpClientLib.h>
boolean syncEventTriggered = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

//Blynk
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
BlynkTimer  BlynkTijd;
WidgetLED   BlynkStatusLed(V4);
static int  BlynkTijdsDuur = 1;
static int  BlynkUur = 18;
static int  BlynkMinuut = 0;
static int  BlynkSeconde = 0;
static long BlynkCountDown = 0;
static bool BlynkTimerArmed = false;

//S20
#include <OneButton.h>
#define PIN_BUTTON       0
#define PIN_BLUELEDRELAY 12
#define PIN_GREENLED     13
OneButton S20Button(PIN_BUTTON, true);

//LED Blink
#include <Ticker.h>
Ticker S20LedTicker;
enum LedBlinkType {
  LED_BLINK_OFF = 0,
  LED_BLINK_ON,
  LED_BLINK_SLOW,
  LED_BLINK_FAST,
  LED_BLINK_1,
  LED_BLINK_2,
  LED_BLINK_3,
  LED_BLINK_4
} LedBlinkState = LED_BLINK_OFF;

char auth[] = "youkey";

void S20LedTickerHandler() {
  static int LedTick = 0;
  bool LedState;
  
  LedTick++;
  if(LedTick >= 10)
    LedTick = 0;
   
  switch(LedBlinkState) {
    case LED_BLINK_OFF:
      LedState = false;
    break;
    case LED_BLINK_ON:
      LedState = true;
    break;
    case LED_BLINK_SLOW:
      if(LedTick / 5)
        LedState = true;
      else
        LedState = false;
    break;
    case LED_BLINK_FAST:
    if(LedTick / 2)
        LedState = true;
      else
        LedState = false;
    break;
    case LED_BLINK_1:
    if(LedTick == 0)
        LedState = true;
      else
        LedState = false;
    break;
    case LED_BLINK_2:
    if((LedTick == 0) || (LedTick == 2))
        LedState = true;
      else
        LedState = false;
    break;
    case LED_BLINK_3:
    if((LedTick == 0) || (LedTick == 2)  || (LedTick == 4))
        LedState = true;
      else
        LedState = false;
    break;
    case LED_BLINK_4:
    if((LedTick == 0) || (LedTick == 2)  || (LedTick == 4) || (LedTick == 6))
        LedState = true;
      else
        LedState = false;
    break;
  }

  if(LedState) {
    digitalWrite(PIN_GREENLED, LOW);
  } else {
    digitalWrite(PIN_GREENLED, HIGH);
  }
}

void S20ButtonRelease() {
  //S20 Button is ingedrukt, tijd verhogen met 1 uur, max 4 uur.
  Serial.println("S20: Button Pressed");
  BlynkCountDown += (60 * 60); 
  if(BlynkCountDown > (4 * 60 * 60))
    BlynkCountDown = 4 * 60 * 60;
}

void S20ButtonLong() {
  Serial.println("S20: Button Long Pressed");
  BlynkCountDown = 0;
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("WiFiManager: Entered config mode");
  Serial.println(WiFi.softAPIP());
  
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  
  LedBlinkState = LED_BLINK_FAST;
}

void BlynkTimerHandler(void) {
  //DEBUG
  time_t tijd;
  struct tm *tm;
  String setTime = String(BlynkUur) + ":" + BlynkMinuut + ":" + BlynkSeconde;

  if(BlynkCountDown)
    BlynkCountDown--;

  //1. Update LCD
  String s = String("Tijdklok actief:\t ");
  if(BlynkTimerArmed)
    s += "Ja";
  else
    s += "Nee";
  s += "; " + setTime + "\nHoe lang nog aan: " + BlynkCountDown / 60 + " m";
  Blynk.virtualWrite(V3, s);

  //2. Controlleer of CountDown actief is
  if(BlynkCountDown) {
    digitalWrite(PIN_BLUELEDRELAY, HIGH);
    BlynkStatusLed.on();
    Blynk.virtualWrite(V2, HIGH);
  } else {
    digitalWrite(PIN_BLUELEDRELAY, LOW);
    BlynkStatusLed.off();
    Blynk.virtualWrite(V2, LOW);
  }

  //3. Controlleer of er een Tijdschakeling actief is
  if(BlynkTimerArmed) {
    tijd = NTP.getTime();
    tm = localtime (&tijd);
    if((tm->tm_hour == BlynkUur) && (tm->tm_min == BlynkMinuut) && (tm->tm_sec >= BlynkSeconde)) {
      BlynkCountDown = (60 * 60) * BlynkTijdsDuur;
      BlynkTimerArmed = false;
    }
  }

  //4. Update Led
  int n;
  n =  BlynkCountDown / 3600;
  n += BlynkCountDown % 3600 ? 1 : 0;
  switch(n) {
    case 0:
      if(BlynkTimerArmed)
        LedBlinkState = LED_BLINK_ON;
      else
        LedBlinkState = LED_BLINK_OFF;
    break;
    case 1:
      LedBlinkState = LED_BLINK_1;
    break;
    case 2:
      LedBlinkState = LED_BLINK_2;
    break;
    case 3:
      LedBlinkState = LED_BLINK_3;
    break;
    case 4:
    default:
      LedBlinkState = LED_BLINK_4;
    break;
  }
}

void processSyncEvent(NTPSyncEvent_t ntpEvent) {
  if (ntpEvent) {
    Serial.print("NTP: Time Sync error: ");
    if (ntpEvent == noResponse)
      Serial.println("Server not reachable");
    else if (ntpEvent == invalidAddress)
      Serial.println("Invalid NTP server address");
  }
  else {
    Serial.print("NTP: Got NTP time: ");
    Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
  }
}

void setup() {
  //Serial
  Serial.begin(115200);
  Serial.println("Start SmartDrankKast 1.0");

  //S20 Hardware
  pinMode(PIN_GREENLED, OUTPUT);
  digitalWrite(PIN_GREENLED, HIGH);
  pinMode(PIN_BLUELEDRELAY, OUTPUT);
  digitalWrite(PIN_BLUELEDRELAY, LOW);
  pinMode(PIN_BUTTON, INPUT);

  //Button
  //attachInterrupt(PIN_BUTTON, ESP8266PinISR, FALLING);
  S20Button.attachClick(S20ButtonRelease);
  S20Button.attachLongPressStart(S20ButtonLong);

  //Led blink
  LedBlinkState = LED_BLINK_SLOW;
  S20LedTicker.attach(0.1, S20LedTickerHandler);

  //WiFiManager
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect()) {
    Serial.println("WiFiManager: failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WiFiManager: connected...");

  //OTA
  //Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("SmartDrankKast");

  ArduinoOTA.onStart([]() {
    Serial.println("ArduinoOTA: Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA: started...");
  Serial.println("ArduinoOTA: Ready");
  Serial.print("ArduinoOTA: IP address: ");
  Serial.println(WiFi.localIP());

  Blynk.config(auth);
  Blynk.connect();

  BlynkTijd.setInterval(1000L, BlynkTimerHandler);

  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    ntpEvent = event;
    syncEventTriggered = true;
  });
  
  NTP.begin("nl.pool.ntp.org", 1, true);
  NTP.setInterval(63);
}

BLYNK_WRITE(V0)
{
  BlynkTijdsDuur = param.asInt(); // assigning incoming value from pin V1 to a variable
  Serial.print("Blynk: Slider waarde is: ");
  Serial.println(BlynkTijdsDuur);
}

BLYNK_WRITE(V1) {
  TimeInputParam t(param);

  if (t.hasStartTime())
  {
    Serial.println(String("Blynk Tijdstip: ") +
                   t.getStartHour() + ":" +
                   t.getStartMinute() + ":" +
                   t.getStartSecond());
    Serial.print("Blynk NTP Tijd");
    Serial.println(NTP.getTimeDateString());
    
    BlynkUur = t.getStartHour();
    BlynkMinuut = t.getStartMinute();
    BlynkSeconde = t.getStartSecond();

    //Activeer timer indien tijdstip en tijdsduur zijn gezet.
    if(BlynkTijdsDuur) {
      BlynkTimerArmed = true;
    } else {
      BlynkTimerArmed = false;
    }
  } else {
    Serial.println("BlynkTijdstip: onjuiste parameter");
  }
}

BLYNK_WRITE(V2) {
  if(param.asInt() == 1)
  {
     BlynkCountDown = (60 * 60) * BlynkTijdsDuur;
  }
  else
  {
     BlynkCountDown = 0;
  }
  
  //BlynkTimerArmed = false;
  Serial.print("Blynk: BlynkCountDown waarde is: ");
  Serial.println(BlynkCountDown);
}

void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  BlynkTijd.run();
  S20Button.tick();

  if (syncEventTriggered) {
    processSyncEvent(ntpEvent); 
    syncEventTriggered = false;
  }  
}
