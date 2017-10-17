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

#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
BlynkTimer  BlynkTijd;
WidgetLED   BlynkStatusLed(V4);
WidgetRTC   BlynkRtc;
static int  BlynkTijdsDuur = 1;
static int  BlynkUur = 18;
static int  BlynkMinuut = 0;
static int  BlynkSeconde = 0;
static long BlynkCountDown = 0;
static bool BlynkTimerArmed = false;

//S20
#include <eBtn.h>
#define PIN_BUTTON       0
#define PIN_BLUELEDRELAY 12
#define PIN_GREENLED     13
eBtn S20Button = eBtn(PIN_BUTTON);

TimedBlink S20StatusLed(PIN_GREENLED);

char auth[] = "YourAuthToken";

void S20ButtonRelease() {
  //S20 Button is ingedrukt, tijd verhogen met 1 uur, max 4 uur.
  BlynkCountDown += (60 * 60); 
  if(BlynkCountDown > (4 * 60 * 60))
    BlynkCountDown = 4 * 60 * 60; 
}

void S20ButtonLong() {
  BlynkCountDown = 0;
}

void ESP8266PinISR(){
  S20Button.handle();
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("WiFiManager: Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //Led blink fast
  S20StatusLed.blink(200,200);
}

void BlynkTimerHandler(void) {
  //DEBUG
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String setTime = String(BlynkUur) + ":" + BlynkMinuut + ":" + BlynkSeconde;

  Serial.print("Current time: ");
  Serial.println(currentTime);

  //1. Update LCD
  String s = String("op tijdstip?: ");
  if(BlynkTimerArmed)
    s += "ja";
  else
    s += "nee";
  s += "; " + setTime + "bezig: " + BlynkCountDown;
  Blynk.virtualWrite(V3, s);

  //2. Controlleer of CountDown actief is
  if(BlynkCountDown) {
    digitalWrite(PIN_BLUELEDRELAY, HIGH);
    BlynkStatusLed.on();
  } else {
    digitalWrite(PIN_BLUELEDRELAY, LOW);
    BlynkStatusLed.off();
  }

  //3. Controlleer of er een Tijdschakeling actief is
  if(BlynkTimerArmed) {
    if((hour() == BlynkUur) && (minute() == BlynkMinuut) && (second() >= BlynkSeconde)) {
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
      S20StatusLed.blinkOff();
    break;
    case 1:
      S20StatusLed.blink(50, 900);
    break;
    case 2:
      S20StatusLed.blink(50, 450);
    break;
    case 3:
      S20StatusLed.blink(50, 300);
    break;
    case 4:
    default:
      S20StatusLed.blink(50, 200);
    break;
  }
}

void setup() {
  //Serial
  Serial.begin(115200);
  Serial.println("Booting SmartDrankKast 1.0");

  //S20 Hardware
  pinMode(PIN_GREENLED, OUTPUT);
  digitalWrite(PIN_GREENLED, HIGH);
  pinMode(PIN_BLUELEDRELAY, OUTPUT);
  digitalWrite(PIN_BLUELEDRELAY, LOW);
  pinMode(PIN_BUTTON, INPUT);

  //S20Button.on("press",pressFunc);
  //S20Button.on("hold",holdFunc);
  S20Button.on("release",S20ButtonRelease);
  S20Button.on("long",S20ButtonLong);
  //setting the interrupt on btn pin to react on change state
  attachInterrupt(PIN_BUTTON, ESP8266PinISR, CHANGE);

  //Led blink slow
  S20StatusLed.blink(600,600);

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
  S20StatusLed.blinkOff();

  //LED off
  digitalWrite(PIN_GREENLED, HIGH);

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

  BlynkTijd.setInterval(1000L, BlynkTimerHandler);
  BlynkRtc.begin();
}

BLYNK_WRITE(V0)
{
  BlynkTijdsDuur = param.asInt(); // assigning incoming value from pin V1 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("Blynk: Slider value is: ");
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
  BlynkCountDown = (60 * 60) * BlynkTijdsDuur;
  BlynkTimerArmed = false;
}

void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  S20StatusLed.blink();
}
