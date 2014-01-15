/*--------------------------------------------------------------*/
/*               Arduino Terrarium Mailer Deamon                */
/*--------------------------------------------------------------*/
/* Temperatur, Relative Luftfeuchte und Füllstandsüberwachung   */
/* Bei überschreibten von Warn oder Alarmtemperaturen eine      */
/* Email an einen oder mehrere Empfaenger versenden.            */
/*                                                              */
/* Die gesammelten Daten werden nach dem Schema MonatsnameJahr  */
/* z.B. DEZ14 im .cvs Format auf der SD-Karte gespeichert.      */
/*                                                              */
/* Maik Vattersen, mvattersen@gmail.com                         */
/*                                                              */
/*--------------------------------------------------------------*/


// Included Libraries (Standard Arduino IDE)
#include <SD.h>
#include <SPI.h>
#include <Ethernet.h>
#include <LiquidCrystal.h>


// Included extern Libraries (NON Standard Arduino IDE)
#include <DS1307.h>  // http://www.henningkarlsen.com/electronics/library.php?id=34
#include <dht11.h>


#define DHT11PIN A8          // DHT11 Pin
#define LCD_LIGHT_PIN A11    // Define LCD Backlight PIN
const byte chipSelect = 4;   // SDCard Pin
const byte triggerPin = 14;  // HC-SR04 Trigger Pin
const byte echoPin    = 15;  // HC-SR04 Echo Pin
const byte rel[]   = {  16,  17,  18,  19 /*,  20,  21,  22,  23 */ }; // Relais Pins 16 heizl, 17 regen, 18 fan, 19 Sparlampe
boolean relState[] = { LOW, LOW, LOW, LOW /*, LOW, LOW, LOW, LOW */ }; // Schaltzustände der Relais Pins

// DS1307:      SDA pin   -> Arduino Digital A1
//              SCL pin   -> Arduino Digital A2
DS1307 rtc(A9, A10);       // SDA, SCL

// Init the LCD
LiquidCrystal lcd(8, 9, 3, 5, 6, 7);

const boolean startMail = true; // Sende Mail nach fehlerfreiem Start
boolean useLogg   = true; // Logging auf SD aktivieren
const byte logg = 60;     // logging interval in Sekconds

// Zeit
const boolean setTime = false;  // Set Time on Startup to given Values 
byte second       = 00, 
     minute       = 36, 
     hour         = 22,
     weekdayVal   = 1,
     day          = 20,
     month        = 12;
unsigned int year = 2013;

// Statusmeldungen (einfach oder mehrfach Angabe) versenden um 
const unsigned int stat[] = { 605 }; // 605 = 6:05 Uhr

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// E-Mail Variablen
IPAddress mailServer( xxx, xxx, xxx, xxx ); // SMTP Server IP
const byte SMTP_PORT = 25; // Port 25 = default
const String benutzer = "YOURMAILUSERNAME";
// In an Terminal type: perl -MMIME::Base64 -e 'print encode_base64("\000USER\@DOMAIN\000PASSWORD")'
const String passwd = "YOURMAILPASSWORD"; 
const String domain = "YOURPROVIDER.TLD";

const String empfaenger[] = {  // Alle Empfaenger der Mail
                        "YOURMAILUSERNAME@YOURPROVIDER.TLD", 
                        "YOUR2MAILUSERNAME@YOUR2PROVIDER.TLD"
};

// Andere Variablen
const byte warnIntervalMaxCount  = 1; // Wie oft soll bei Warnung eine Mail versendet werden
const byte alarmIntervalMaxCount = 2; // Wie oft soll bei Alarm eine Mail versendet werden

const byte warnTemp    = 35;   // Warntemperatur in °C (min 1, max 55)
const byte alarmTemp   = 5;    // Alarmtemperatur °C auf Warntemp aufaddiert
const byte warnTempLow = 20;   // Warntemperatur Unterwert in °C (wird von warnTemp abgezogen)

const byte warnWet     = 95;   // Warnfeuchte in % (min 1, max 100)
const byte alarmWet    = 5;    // Alarmfeuchte % auf Warnfeuchte aufaddiert
const byte warnWetLow  = 50;   // Warnfeuchte Unterwert in % (wird von warnWet abgezogen)

byte minFillDay        = 180;  // Gesammtfassungsvermögen des Tanks in dezilitern (0-255)
const byte warnFill    = 4;    // Warnfüllstand in litern, wenn Wert unterschritten
const byte alarmFill   = 2;    // Alarmfüllstand in litern, wenn Wert unterschritten
const byte reFill      = 8;    // Nachfüllhinweis in Mail in litern, wenn Wert unterschritten

const byte loggCounterMax = 5;  // Maximale Anzahl der schreibversuche auf SDKarte bevor logging disabled wird
const byte sendCounterMax = 5;  // Maximale Anzahl gesendeter Mails (wird Stündlich resetet)


// Relais 1, rel[0], Heizlampe 
const byte heat[] = {
    13,  // Startstunde 
    17,  // Stopstunde
    32   // maxTemperatur in °C bevor automatisch abgeschaltet wird
};
// Relais 2, rel[1], Beregnung 
const byte rain[] = {
    31,  // StartMinute 
    32,  // StopMinute
    9,   // SchaltStunde
    59   // dauer in Sekunden (59 = Minutengenau)
};
// Relais 3, rel[2]
//const byte name[] = {
//    0,  // Start
//    1,  // Stop
//    1   // Value
//};
// Relais 4, rel[3]
const byte light[] = {
    10,  // Start
    20,  // Stop
    33   // maxTemperatur in °C bevor automatisch abgeschaltet wird
};


// Minutly Arrays
byte  tempArrM[10];

// Hourly Arrays
byte   tempArr[10];
byte    wetArr[10];
byte   fillArr[10];
byte  grundArr[10];

// Daily Arrays
byte   tempArrD[7];
byte    wetArrD[7];
byte   fillArrD[7];


//
////
//// fixed Values, dont edit
////
//


// bytes
// Minutly Array Vars
byte tempArrMCounter = 0;
byte    tempArrMSize = 0;
// hourly ArrayVars
byte  tempArrCounter = 0;
byte     tempArrSize = 0;
byte   wetArrCounter = 0;
byte      wetArrSize = 0;
byte  fillArrCounter = 0;
byte     fillArrSize = 0;
byte grundArrCounter = 0;
byte    grundArrSize = 0;
// daily ArrayVars
byte tempArrDCounter = 0;
byte    tempArrDSize = 0;
byte  wetArrDCounter = 0;
byte     wetArrDSize = 0;
byte fillArrDCounter = 0;
byte    fillArrDSize = 0;
byte warnIntervalCount  = 0;
byte alarmIntervalCount = 0;
byte      minTempDay = 85;          // initialwerte
byte      minWetDay  = 99;          // initialwerte
byte      maxFillDay = minFillDay;  // initialwert gleich Tankfassungsangabe minFillDay = 18l
byte initial = 1;
byte grund, feuchte, temperaturC, errCode, mini, maxi, steps, 
     bericht, sendCounter, loggCounter, counter, 
     maxTempDay, maxWetDay, /* Counter Vars */ i, j;

// byte arrays
byte startDate[] = { 0, 0, 0, 0, 0, 0 }; // Tag, Monat, Jahr, Wochentag, Stunde, Minute
byte laufZeit[]  = { 0, 0, 0, 0 };       // Wochen, Tage, Stunden, Minuten


// longs
long prevMillis = 0;
long prevMillisM = 0;
long prevMillisH = 0;
long prevMillisD = 0;
long updateTime;
const long interval = 60000;     // 1min
//const long warnInterval  = 28800000; // 8h 
const long warnInterval  = 300000; // 5min
const long alarmInterval = 120000; // 2min


// floats
float liter;

// bools
boolean checked, logging, logged, clientConnected, hbeat, backlight, clearer;
boolean stats = false;
boolean versenden = false;

// ints
int displayVar;
// define some values used by the LCD panel and buttons 
unsigned int lcd_key     = 0; 
unsigned int adc_key_in  = 0; 
#define btnRIGHT  0 
#define btnUP     1 
#define btnDOWN   2 
#define btnLEFT   3 
#define btnSELECT 4 
#define btnNONE   5 

// LCD Symbole
// Ventilator
byte C0[8] = { B10000, B11001, B11011, B01111, B00100, B00100, B01100, B11100 };
// Mail Symbol
byte C1[8] = { B00000, B00000, B11111, B10101, B10001, B11111, B00000, B00000 };
// Fill (Wellen)
byte C2[8] = { B01100, B10011, B00000, B00110, B11001, B00000, B01100, B10011 };
// Temp (Termometer)
byte C3[8] = { B00100, B01010, B01010, B01010, B01110, B10111, B11111, B01110 };
// Wet (Tropfen)
byte C4[8] = { B00100, B00100, B01110, B01010, B10111, B10111, B11111, B01110 };
// Mond
byte C5[8] = { B00110, B01100, B11000, B11000, B11000, B01100, B00110, B00000 };
// Sonne
byte C6[8] = { B00100, B10001, B00100, B01010, B01010, B00100, B10001, B00100 };
// Energielampe
byte C7[8] = { B00100, B01010, B01010, B01010, B01010, B00100, B10001, B00100 };


// Objekt Instanzierungen
EthernetClient client; // Ethernet Client Object
Time time;   // Declare a DC1307 Object
dht11 DHT11; // Declare a DHT11 Object


// ---------- Setup ---------- //
void setup() {
    
  // Turn on Backlight
  pinMode(LCD_LIGHT_PIN, OUTPUT);
  digitalWrite(LCD_LIGHT_PIN, HIGH);

  // Ventilator
  lcd.createChar(1,C0);
  
  // Mail
  lcd.createChar(3,C1);
  
  // Fill,- Temp,- Wetsymbols
  lcd.createChar(4,C2); // Fill Wellen
  lcd.createChar(5,C3); // Temp Termometer
  lcd.createChar(6,C4); // Wet Tropfen
  
  // Mond
  lcd.createChar(7,C5);
  
  // Energiesparlampe 
  lcd.createChar(2,C7);
  
  // Sonne 
  lcd.createChar(8,C6);
  
  // Setup LCD to 16x2 characters
  lcd.begin(16, 2);
  
  lcd.clear();
  lcd.setCursor(0 ,0);
  lcd.print("ArduinoTempMailD");
    
  // Start the Serial library:
  Serial.begin(9600);
  
  Serial.println("--------------------------------------------------");
  
  Serial.print("  ATM Arduino Temperatur Mailerdaemon Version 2.0");
  Serial.print("  Reportmails: ");
  
  for(i = 0; i < (sizeof(stat)/sizeof(unsigned int)); i++) {
    Serial.print(split(stat[i],0));
    Serial.print(":");
    Serial.print(split(stat[i],1)/10);
    Serial.print(split(stat[i],1)%10);
    if( (sizeof(stat)/sizeof(int) -1) != i ) Serial.print(", ");
  }
  
  Serial.println();
  Serial.println("--------------------------------------------------");
  Serial.println();
  
  lcd.setCursor(1 ,1);
  
  lcd.write(1);
  lcd.write(32);
  delay(500);
  lcd.write(3);
  lcd.write(32);
  delay(500);
  lcd.write(4);
  lcd.write(32);
  delay(500);
  lcd.write(5);
  lcd.write(32);
  delay(500);
  lcd.write(6);
  lcd.write(32);
  delay(500);
  lcd.write(7);
  lcd.write(32);
  delay(500);
  lcd.write(2);
  lcd.write(32);
  delay(500);
  lcd.write(8);
    
  Serial.println("Initializing RAM...");
  Serial.print("SUCCESS - ");
  Serial.print( availableMemory() );
  Serial.print(" bytes free / ");
  Serial.print( 8192 - availableMemory() );
  Serial.print(" bytes used");
  Serial.println("\n");
  delay(2000);
  
  lcd.setCursor(0 ,1);
  lcd.print("  version 2.0   ");
  delay(2000);
  
  // scroll leftout
  for (i = 0; i < 16; i++) { lcd.scrollDisplayLeft(); delay(100); }
  
//  delay(3000);
  lcd.clear();
  
  lcd.setCursor(0 ,0);
  lcd.print("Reportmails:    ");
  lcd.setCursor(0 ,1);
  for(i = 0; i < (sizeof(stat)/sizeof(unsigned int)); i++) {
    lcd.print(split(stat[i],0));
    lcd.print(":");
    lcd.print(split(stat[i],1)/10);
    lcd.print(split(stat[i],1)%10);
    if( (sizeof(stat)/sizeof(int) -1) != i ) lcd.print(" ");
    if( (sizeof(stat)/sizeof(int)) > 2 && i > 2 ) { lcd.setCursor(13 ,0); lcd.print("+"); lcd.print( ( sizeof(stat) / sizeof(int) ) -3 ); lcd.print(" "); }
  }
  delay(2000);
  lcd.clear();
  
  // initialize SD card
  Serial.println("Initializing SD card...");
  lcd.setCursor(0 ,0);
  lcd.print("Init SD card...");
  lcd.setCursor(0 ,1);
  if (!SD.begin(chipSelect)) {
    Serial.println("ERROR - SD card initialization failed!");
    lcd.print("SD card failed!");
    
    delay(3000);
    lcd.setCursor(0 ,1);
    lcd.print("Logging disabled");
    
    // Error
    errCode = 2; // 2 = SD Card initialising failed
    useLogg = false; // disable logging
    
    //return;    // init failed
  } else {
    Serial.println("SUCCESS - SD card initialized.");
    lcd.print("SD card success");
  }
  
  Serial.println();
  delay(2000);
  lcd.clear();
  
  lcd.setCursor(0 ,0);
  lcd.print("Init Ethernet IP");
  lcd.setCursor(0 ,1);
  // start the Ethernet connection by DHCP (Be Sure to have your Router configured!!!):
  Serial.println("Initializing Ethernet connection...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("ERROR - Failed to configure Ethernet using DHCP.");
    // Error
    errCode = 5; // 5 = Net DHCP failed, fixed use
    lcd.print("fail DHCP ");
    
  } else {
    Serial.println("SUCCESS - Ethernet successfuly configure by DHCP");
    lcd.print(" ");
    lcd.print(Ethernet.localIP());
  }
  
  Serial.print("SUCCESS - Server ");
  Serial.print(Ethernet.localIP());
  Serial.println(" online ");
  Serial.println();
  
  Serial.println("Initializing RTC Clock...");
  // Set the DS1307 clock to run-mode
  rtc.halt(false);
  Serial.println("SUCCESS - RTC Clock in run-mode");
  Serial.println();
  
  // RelaisPins PinModes and set powerdown per default
  Serial.println("Initializing Relais");
  for(i = 0; i < (sizeof(rel)/sizeof(byte)); i++) {
    pinMode(rel[i], OUTPUT);
    Serial.print("SUCCESS - Relais #");
    Serial.print(i);
    Serial.print(" Online ");
    digitalWrite(rel[i], relState[i]);
    Serial.println("and set LOW");
  }
    
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println();
  Serial.println("--------------------------------------------------");
  Serial.println("  Setup initializing complete, enter main loop");
  

  Serial.print("  please wait one interval (");
  Serial.print( interval / 1000 );
  Serial.println(" Seconds)");
  Serial.println("--------------------------------------------------");
  Serial.println();
  delay(1000);
  lcd.clear();
  
  lcd.setCursor(2 ,1);
  lcd.print("loading");

}


// ---------- Loop ---------- //
void loop() { 
  
  // Initialisierungs Schleife
  if(initial == 1) {
    if(startMail) { versenden = true; grund = 5; } // Startmail versenden
    
    // Zeit umstellen 
    if(setTime){
      rtc.setDOW(weekdayVal);       // Set Day-of-Week to SUNDAY
      rtc.setTime(hour, minute, second);     // Set the time to 12:00:00 (24hr format)
      rtc.setDate(day, month, year);   // Set the date to October 3th, 2010
    } else {
      // Zeit holen
      time = rtc.getTime();
      second     = time.sec;  // Sekunden
      minute     = time.min;  // Minuten
      hour       = time.hour; // Stunden
      weekdayVal = time.dow;  // Wochentag (Zahlwert, 1)
      day        = time.date; // Kalender Tag
      month      = time.mon;  // Kalender Monat
      year       = time.year; // Kalender Jahr
    }
    
    // Init ... LCD
    for(i = 0; i < 5; i++){
      lcd.setCursor(9 + i ,1);
      lcd.print(".");
      delay(200);
    }
    
    
    
    delay(1000);
    lcd.clear();

    // Serial print Legend and first Data
    Serial.println("Datum                  Senden/Logg  Sensorwerte     Arrays    Zeit   RAM");
    Serial.println("DD dd.mmm.yy hh:mm:ss  Sen|Gru|Log  Temp|RL%|Fuell  Mi|Ho|Da  hh:mm  free/used");
    Serial.println();

    startDate[0] = day;
    startDate[1] = month;
    startDate[2] = split(year,1);
    startDate[3] = weekdayVal;
    startDate[4] = hour;
    startDate[5] = minute;
    
    initial = 0;
  } // ENDE Initialisierungs Schleife
  
  // Intervall 1sek
  unsigned long currMillis = millis();
  if( currMillis - prevMillis > 1000 ){
    prevMillis = currMillis;
      
    //
    // Main TIME
    // Get Time from the DS1307 Tiny RTC
  
    time = rtc.getTime();
    
    second     = time.sec;  // Sekunden
    minute     = time.min;  // Minuten
    hour       = time.hour; // Stunden
    weekdayVal = time.dow;  // Wochentag (Zahlwert, 1)
    day        = time.date; // Kalender Tag
    month      = time.mon;  // Kalender Monat
    year       = time.year; // Kalender Jahr
    
    // Ende Main TIME 
    //
    
    int chk = DHT11.read(DHT11PIN);
    
    // Sensorwerte alle 5 Sek abfragen
    if( second % 5 == 0 ) {
      temperaturC = DHT11.temperature;
      feuchte = DHT11.humidity;
      liter = (( 18.0 / 32.0 ) * ( mapfloat(fill(triggerPin,echoPin),0,42,42,0) - 4.5 ) );
    }

    // Refresh Display
    
    if( displayVar == 0 ) dots(2,1); // Show flashing Seconddots 
    
    // Anzeige Daten LCD Screens und Sekundengenaue Relais Schaltungen
    if( displayVar < 2 ) {
      
      heart();        // Show flashing Heartbeatsymbols
      
      if( !useLogg ) { lcd.setCursor(14,0); lcd.write(char(208)); clearer = true; } // SD-Kartenfehler Symbol
        else { clearer = false; } 
      
      // Ventilator Anzeige und Schaltung
      if( temperaturC > ( warnTemp - 2 ) ) { 
        lcd.setCursor(14,0); 
        lcd.write(1); 
        clearer = true; 
        relState[2] = HIGH;
        digitalWrite(rel[2], relState[2]);
      } else { 
        if(useLogg) clearer = false; 
        relState[2] = LOW;
        digitalWrite(rel[2], relState[2]);
      } 
            
      // Relais 1, Heizlampenanzeige
      if( relState[0] ) { lcd.setCursor(14,0); lcd.write(8); clearer = true; } // Heizlampe (Sonne)
         else { if( temperaturC > ( warnTemp - 2 ) ) clearer = false; }

      // Relais 2, Beregnungsanlage, Anzeige und Schaltung (Sekundengenau, daher hier), an Wochentagen 2,4,6,7 (Di,Do,Sa,So)
      if( !relState[0] && // Nur wenn die Heizlampe aus ist
          ( ( hour == rain[2] && ( minute >= rain[0] && minute < rain[1] ) ) &&  // Schaltzeit
          ( second >= 0 && second < rain[3] ) &&  // Check for seconds 
          liter > 0 && // Nur wen noch genug im Tank
          ( weekdayVal == 2 || weekdayVal == 4 || weekdayVal == 6 || weekdayVal == 7 ) ) ) { // Nur di, do, sa, so
        lcd.setCursor(14,0); 
        lcd.write(6);
        clearer = true;
        relState[1] = HIGH;
        digitalWrite(rel[1], relState[1]);
      } else { 
        if( !relState[0] ) clearer = false;
        relState[1] = LOW;
        digitalWrite(rel[1], relState[1]); // ausschalten wenn ausserhalb der Schaltzeit
      }
            
      // Clear 14,0
      if(!clearer) { lcd.setCursor(14,0); lcd.write(" "); } 
      
      // Display Moon at Night, 22 to 6 o'clock
      if( hour >= 22 || hour < 6 ) {
        lcd.setCursor(12,0); lcd.write(7); // Mond
        // Turn Backlight off
        if( backlight && ( ( hour == 21 && minute == 32 ) && second == 1 ) ) { 
          digitalWrite(LCD_LIGHT_PIN, LOW);
          backlight = false; 
        }
      } else { 
        if( relState[3] ) { lcd.setCursor(12,0); lcd.write(2); } // Energiesparlampe
         else { lcd.setCursor(12,0); lcd.write(" "); }
        // Turn Backlight on
        if( !backlight && hour == 6 && minute == 07 && second == 1 ) { 
          digitalWrite(LCD_LIGHT_PIN, HIGH);
          backlight = true; 
        }
      } // ENDE Display Moon
      
    } // ENDE DisplayVar < 2
    
    // Zweiter Screen
    if( displayVar == 1 ) dots(6,1);
    
    showStatLCD();      
    
    // Laufzeit hochzählen 
    if( laufZeit[3] > 59 ) {
      laufZeit[2] += 1;
      laufZeit[3] = 0;
    } 
    if( laufZeit[2] > 23 ) {
      laufZeit[1] += 1;
      laufZeit[2] = 0;
    }
    if( laufZeit[1] > 6 ) {
      laufZeit[0] += 1;
      laufZeit[1] = 0;
    }
    
  } // ENDE Intervall 1sek
  
  //
  // LOGGING Part
  //

  if( logging && useLogg && !versenden ) {
    
    if( loggCounter < loggCounterMax ) {
      
      String logFileStr = "";
      File dataFile;

      // Generate Filename like Dez13.csv (MMMYY.CSV)
      logFileStr += monthName(month);                     // Month, 3 Letter
      logFileStr += split(year,1);                        // Year, 2 Letter
      char logFile[sizeof(logFileStr)];                   // SetSize
      logFileStr.toCharArray(logFile, sizeof(logFile));   // Convert String 2 CharArray
      
      // Validate existence of File
      if(!SD.exists(logFile)) {
        // Try to create File
        dataFile = SD.open(logFile, FILE_WRITE);
        dataFile.close();
        // Validate existence of File or disable logging
        if(!SD.exists(logFile)) {
          Serial.print("Cannot create ");  
          Serial.print(logFileStr);
          Serial.println(", logging disabled");
          useLogg = false;
        } 
      }
           
      dataFile = SD.open(logFile, FILE_WRITE);
      
      if (dataFile) {
        dataFile.print(day);
        dataFile.print(".");
        dataFile.print(month);
        dataFile.print(".");
        dataFile.print(split(year,1));
        dataFile.print(" ");
        dataFile.print(hour);
        dataFile.print(":");
        dataFile.print(minute/10);
        dataFile.print(minute%10);
        dataFile.print(":");
        dataFile.print(second/10);
        dataFile.print(second%10);
        dataFile.print(";");
        if( minute % logg == 0 ) { dataFile.print("log"); } else { dataFile.print( grund ); }
        dataFile.print(";");
        dataFile.print(temperaturC);
        dataFile.print(";");
        dataFile.print(feuchte);
        dataFile.print(";");
        dataFile.print(liter);
        dataFile.println();
        dataFile.close();
        logged = true;
        logging = false;
        Serial.print(hour);
        Serial.print(":");
        Serial.print(minute/10);
        Serial.print(minute%10);
        Serial.print(":");
        Serial.print(second/10);
        Serial.print(second%10);
        Serial.print(" - Successfully logged to ");
        Serial.print(logFileStr);
        Serial.println(" file!");
      } else {
        // Error open File
        Serial.print("error opening ");
        Serial.print(logFileStr);
        Serial.println(" file!");
        // Error
        errCode = 3; // 3 = SD write File fail
        loggCounter++;
        logged = false;
        logging = true;
      }
    } else {
      // MaxLogCount reached
      Serial.print("Max Logg Counter reached, ");
      Serial.println("Logging disabled!");
      logged = true;
      logging = false;
    }

  } // Ende if logging
  
  
   
  // Intervall einmal Minütlich (60000 Sekunden)
  unsigned long currMillisM = millis();
  if( currMillisM - prevMillisM > 60000 ){
    prevMillisM = currMillisM;
    
    laufZeit[3]++;
        
    checked = false; // prüfen wieder zulassen
    
    // 
    ////
    //// Relais Schalten
    ////
    //
    
    // Heizstrahler 
    if( ( hour >= heat[0] && hour < heat[1] ) && temperaturC < heat[2] ){
      relState[0] = HIGH;
      digitalWrite(rel[0], relState[0]);
    } else { 
      relState[0] = LOW;
      digitalWrite(rel[0], relState[0]); // ausschalten wenn ausserhalb der Schaltzeit
    } 
    
    
    // Energiesparlampe
    if( ( hour >= light[0] && hour < light[1] ) && temperaturC < light[2] ){
      relState[3] = HIGH;
      digitalWrite(rel[3], relState[3]);
    } else { 
      relState[3] = LOW;
      digitalWrite(rel[3], relState[3]); // ausschalten wenn ausserhalb der Schaltzeit
    }
       
    //
    //// ENDE Relais Schalten
    //
        
    if( temperaturC > maxTempDay ) maxTempDay = temperaturC; // Maxwert 
    if( temperaturC < minTempDay ) minTempDay = temperaturC; // Minwert
    if( feuchte > maxWetDay ) maxWetDay = feuchte; // Maxwert merken
    if( feuchte < minWetDay ) minWetDay = feuchte; // Minwert merken
    if( liter > ( maxFillDay / 10 ) ) maxFillDay = liter * 10; // Maxwert merken
    if( liter < (minFillDay / 10.0) ) minFillDay = liter * 10; // Minwert merken
      
    tempArrMSize = (sizeof(tempArrM)/sizeof(byte)); // Arraygröße
      // Wenn Zähler größer oder gleich ArraySize
      if( tempArrMCounter >= tempArrMSize ){
        // Array ist komplett befüllt
        for(i = 0; i < tempArrMSize; i++) {
          if( ( i + 1 ) < tempArrMSize ){
            // umschreiben
            tempArrM[i] = tempArrM[i + 1];
          } else {
            // und letzten aktuellen Wert einsetzen
            tempArrM[i] = temperaturC;
          }
        } // ENDE for
      } else {
        // Array mit Counter als index weiter befüllen
        tempArrM[tempArrMCounter]  = temperaturC;
        // und danach Zähler einen weiter setzen
        tempArrMCounter++;
      }    
    
  } // Intervall Minütlich
  
  
  
  // Intervall einmal Stundlich (3600000 Sekunden)
  unsigned long currMillisH = millis();
  if( currMillisH - prevMillisH > 3600000 ){
    prevMillisH = currMillisH;
    
    // reset Sending counter
    sendCounter = 0;
    
    useLogg = true; // reset faillogging
    
    if ( hour == 6 && minute == 05 ) {
      warnIntervalCount = 0;
      alarmIntervalCount = 0;
    }
    
    
    // Temperatur Array befüllen
    tempArrSize = (sizeof(tempArr)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( tempArrCounter >= tempArrSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for(i = 0; i < tempArrSize; i++) {
        if( ( i + 1 ) < tempArrSize ){
          // Solange alte werte versetzt einfüllen bis letzte Stelle erreicht ist
          tempArr[i] = tempArr[i + 1];
        } else {
          // letze Stelle, aktuellen Wert einfügen
          tempArr[i] = temperaturC;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      tempArr[tempArrCounter]  = temperaturC;
      // und danach Zähler einen weiter setzen
      tempArrCounter++;
    } // Ende Temperatur Array befüllen
    
    
    // Feuchtigkeits Array befüllen
    wetArrSize = (sizeof(wetArr)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( wetArrCounter >= wetArrSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for( i = 0; i < wetArrSize; i++) {
        if( ( i + 1 ) < wetArrSize ){
          wetArr[i] = wetArr[i + 1];
        } else {
          wetArr[i] = feuchte;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      wetArr[wetArrCounter]  = feuchte;
      // und danach Zähler einen weiter setzen
      wetArrCounter++;
    } // Ende Feuchtigkeits Array befüllen
    
    
    // Füllstands Array befüllen
    fillArrSize = (sizeof(fillArr)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( fillArrCounter >= fillArrSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for( i = 0; i < fillArrSize; i++) {
        if( ( i + 1 ) < fillArrSize ){
          fillArr[i] = fillArr[i + 1];
        } else {
          fillArr[i] = liter * 10;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      fillArr[fillArrCounter] = liter * 10;
      // und danach Zähler einen weiter setzen
      fillArrCounter++;
    } // Ende Füllstands Array befüllen
    
  } // ENDE Intervall Stundlich
    
  
  
  // Intervall einmal Täglich (86400000 Sekunden)
  unsigned long currMillisD = millis();
  if( currMillisD - prevMillisD > 86400000 ){
    prevMillisD = currMillisD;
        
    // TemperaturD Array befüllen
    tempArrDSize = (sizeof(tempArrD)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( tempArrDCounter >= tempArrDSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for( i = 0; i < tempArrDSize; i++) {
        if( ( i + 1 ) < tempArrDSize ){
          tempArrD[i] = tempArrD[i + 1];
        } else {
          tempArrD[i] = temperaturC;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      tempArrD[tempArrDCounter]  = temperaturC;
      // und danach Zähler einen weiter setzen
      tempArrDCounter++;
    } // Ende Temperatur Array befüllen
    
    
    // Feuchtigkeits Array befüllen
    wetArrDSize = (sizeof(wetArrD)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( wetArrDCounter >= wetArrDSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for( i = 0; i < wetArrDSize; i++) {
        if( ( i + 1 ) < wetArrDSize ){
          wetArrD[i] = wetArrD[i + 1];
        } else {
          wetArrD[i] = feuchte;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      wetArrD[wetArrDCounter]  = feuchte;
      // und danach Zähler einen weiter setzen
      wetArrDCounter++;
    } // Ende Feuchtigkeits Array befüllen
    
    
    // Füllstands Array befüllen ( Arr1[], Arr2[], ArrCounter, ArrSize, ArrValue )
    fillArrDSize = (sizeof(fillArrD)/sizeof(byte)); // Arraygröße
    // Wenn Zähler größer oder gleich ArraySize
    if( fillArrDCounter >= fillArrDSize ){
      // Array ist komplett befüllt, umschreiben und letzten aktuellen Wert einsetzen
      for( i = 0; i < fillArrDSize; i++) {
        if( ( i + 1 ) < fillArrDSize ){
          fillArrD[i] = fillArrD[i + 1];
        } else {
          fillArrD[i] = liter * 10;
        }
      } // ENDE for
    } else {
      // Arrays mit Counter als index weiter befüllen
      fillArrD[fillArrDCounter]  = liter * 10;
      // und danach Zähler einen weiter setzen
      fillArrDCounter++;
    } // Ende Füllstands Array befüllen
    
  } // ENDE Intervall Täglich


  // Error
  if( weekdayVal <= 0 || weekdayVal >= 8 ) { errCode = 1; } // 1 = RTC getTime fail test 
 
 
  // Wenn Mail versendet werden soll  
  if ( versenden ) {
    // mindestens interval Zeit abwarten
      if( ( ( millis() - updateTime ) >= alarmInterval ) ||  ( ( millis() - updateTime ) == warnInterval ) || ( ( millis() - updateTime / 1000 ) == ( interval / 1000 ) ) ) {
        updateTime = millis();
        if( checked ) { 
          if( alarmIntervalCount < alarmIntervalMaxCount ) {
            if( warnIntervalCount < warnIntervalMaxCount ) {
              if( sendCounter < sendCounterMax ) {
                // Mailversenden und false nach checked (gibt false zurück, wenn erfolgreich versendet) schreiben
                checked = mailSenden();
                // Bei Fehlern Counter hochzählen
                if( checked ) {
                  sendCounter++;
                } else {
                  // Versand erfolgt, Grund Array befüllen nach jedem neuen Versenden
                  grundArrSize = (sizeof(grundArr)/sizeof(byte)); // Arraygröße
                  if( grundArrCounter >= grundArrSize ){
                    for(i = 0; i < grundArrSize; i++) {
                      if( ( i + 1 ) < grundArrSize ){
                        grundArr[i] = grundArr[i + 1];
                      } else {
                        grundArr[i] = grund;
                      }
                    } 
                  } else {
                    grundArr[grundArrCounter] = grund;
                    grundArrCounter++;
                  }
                }
                logging = true;
              } else {
                Serial.println("Max Send Counter reached");
                checked = false;
                versenden = false;
              } 
              if( grund == 6 || grund == 3 || grund == 1 ) warnIntervalCount++;
            } else {
              checked = false;
              versenden = false;
            }
            if( grund == 7 || grund == 4 || grund == 2 ) alarmIntervalCount++;
          } else {
            checked = false;
            versenden = false;
          }
        } // ENDE checked
      }

  } // ENDE versenden
  


  //
  // Pruefinterval Part
  //

  if( ( ( millis() - updateTime ) > interval ) && !checked ) {
          
    if ( minute % logg == 0 /* && second == 1 */ ) {
      logging = true; // hourly logging
    }
    
    // Tagesberichte versenden
    if( ( temperaturC < warnTemp ) && ( feuchte < warnWet )  ) {
      for( i = 0; i < (sizeof(stat)/sizeof(int)); i++) {
        if( ( join(hour,minute) > ( stat[i] - 1 ) ) && ( join(hour,minute) < ( stat[i] + 1 ) ) ) {
          versenden = true;
          grund = 0;
          bericht = i;
        } 
      }
    }
    
    // Füllstands Warn und Alarm auslöser
    if( liter < alarmFill ) {
      versenden = true;
      grund = 7;
    } else if( liter < warnFill ) {
        versenden = true;
        grund = 6;
    } 
    
    // Feuchtigkeits Warn und Alarm auslöser
    if( feuchte > (alarmWet + warnWet) ) {
      versenden = true;
      grund = 4;
    } else if( feuchte > warnWet ) {
        versenden = true;
        grund = 3;
    } 
    
    // Temperatur Warn und Alarm auslöser
    if( temperaturC > (alarmTemp + warnTemp) ) {
      versenden = true;
      grund = 2;
    } else if( temperaturC > warnTemp ) {
        versenden = true;
        grund = 1;
    } 
        
    showStat();
        
    checked = true;

  } // ENDE Pruefinterval
  
  
  checkButtons();
  
  errCode = 0;
  
} // Ende loop



// ---------- Methoden ---------- //

// E-Mail versenden
boolean mailSenden(){

    // if you get a connection, report back via serial:
    if (client.connect(mailServer, SMTP_PORT)) {
      
      if (client.connected()) {

        // get some values new wenn send
        byte fuellstand = (int)( map(fill(triggerPin,echoPin),0,42,42,0) - 4.5 );
        String colorized = "000000";        
        float grundVal = 0.0;
        
        if( grund == 0 ) grundVal = temperaturC;
        if( grund == 1 ) grundVal = temperaturC - warnTemp;
        if( grund == 2 ) grundVal = temperaturC - (warnTemp + alarmTemp);
        if( grund == 3 ) grundVal = feuchte - warnWet;
        if( grund == 4 ) grundVal = feuchte - (warnWet + alarmWet);
        if( grund == 6 || grund == 7 ) grundVal = liter;
        if( grund > 6 | grund < 0 ) grundVal = temperaturC;
        
        if( grund == 0 ) colorized = "00800F"; // OK grün
        if( grund == 1 || grund == 3 || grund == 6 ) colorized = "FF7300"; // warn orange
        if( grund == 2 || grund == 4 || grund == 7 ) colorized = "FF0000"; // alarm rot
        if( grund == 5 ) colorized = "008FCB"; // Initaial hellblau
        if( grund > 7 | grund < 0 ) colorized = "000000"; // default Schwarz
        
        Serial.print("connected to Mailserver ( ");
        Serial.print(mailServer);
        Serial.print(":");
        Serial.print(SMTP_PORT);
        Serial.print(" ");
        Serial.print(domain);
        Serial.print(" ) ");
        
        // Begin SMTP request:
        client.print("EHLO ");
        client.println(domain);
        // Authentication
        client.print("AUTH PLAIN ");
        client.println(passwd);
        // Headerdaten
        client.print("MAIL FROM:<");
        client.print(benutzer);
        client.print("@");
        client.print(domain);
        client.println(">"); // Absender 
        
        // Alle Empfänger aus dem Array verwenden
        for( i = 0; i < (sizeof(empfaenger)/sizeof(String)); i++) {
          client.print("RCPT TO:<");
          client.print(empfaenger[i]);
          client.println(">");
        }
        
        // Mailbody
        client.println("DATA");
        // Antworten an:
        client.print("TO: <");
        client.print(benutzer);
        client.print("@");
        client.print(domain);  // Antwortadresse
        client.println(">");
        // Betreffzeile (innerhalb der Listenansicht)
        client.print("From: ");
        client.print("Arduino ATM ");
        
        // Absender in Betreffzeile
        client.print(" <");
        client.print(benutzer);
        client.print("@");
        client.print(domain);
        client.println(">");
        
        client.println("Content-Type: text/html");
        client.println("Content-Transfer-Encoding: 8bit");
        
        // Betreffzeile (in der Mailansicht)
        client.print("SUBJECT: ");
        if( grund == 0 ) client.print("Bericht");
        if( grund == 1 || grund == 2 ) { client.print("Temperatur "); client.print((int)grundVal); client.print("°C ueberschritten"); }
        if( grund == 3 || grund == 4 ) { client.print("Rel. Luftfeuchtigkeit "); client.print((int)grundVal); client.print("% ueberschritten"); }
        if( grund == 5 ) client.print("ATM wurde gestartet");
        if( grund == 6 || grund == 7 ) { client.print("Füllstand auf "); client.print(liter); client.print(" Liter abgesunken"); }
        if( grund > 7 | grund < 0 ) client.print("Anderer Absendegrund");
        client.println();
        
        // Nachrichtenbody 
        client.println("<!DOCTYPE html>");
        client.println("<html lang='de'>");
        client.println("<head>");
        client.println("<meta http-equiv='content-type' content='text/html; charset=utf-8' />");
        client.println("<style type='text/css'>");
        client.println("  body { background:#CCCCCC;color:#000000;width:95% !important; text-align:center; }");
        client.println("  hr { background-color: #CCCCCC;border: 0; height: 1px;  }");
        client.print("  #wrap { background:#FFFFFF;border-radius:5px;box-shadow:0 5px 5px #000000; ");
        client.println("border:1px solid #999999;padding:5px 5%;margin:20px 5% 10px; }");
        client.print("  .intext { text-align:left;border-top:1px solid #CCCCCC; width:100%; ");
        client.println("border-bottom:1px solid #CCCCCC;padding:15px 0; margin-bottom: 15px; }");
        client.print("  .highlighted { font-size:600%;text-align:center;text-shadow:0 0 10px #000000; color: #");
        client.print(colorized);
        client.println("; }");
        client.println("  .small { font-size:14pt; }");
        client.println("  .middle { clear:both; }");
        client.println("  .left { float:left; }");
        client.println("  .gchart { margin:20px 5%; width:90%; }");
        client.println("</style>");
                
        client.print("<title>");
        if( grund == 0 ) client.print("Bericht");
        if( grund == 1 || grund == 3 ) client.print("WARN ");
        if( grund == 2 || grund == 4 ) client.print("ALARM ");
        if( grund == 1 || grund == 2 ) { client.print("Temperatur um "); client.print(grundVal); client.print("&deg;C ueberschritten"); }
        if( grund == 3 || grund == 4 ) { client.print("Rel. Luftfeuchtigkeit um "); client.print(grundVal); client.print("% ueberschritten"); }
        if( grund == 6 || grund == 7 ) { client.print("Füllstand auf "); client.print(liter); client.print(" Liter abgesunken"); }
        if( grund == 5 ) client.print("ATM wurde gestartet");
        if( grund > 7 | grund < 0 ) client.print("Anderer Absendegrund");
        client.println("</title>");
        
        client.println("</head>");
        client.println("<body>");
        
        client.println("<div id='wrap'>");
        
        client.print("<h2 class='middle'>");
        
        if( grund == 0) { 
          if( bericht == 99 ){
            client.print("Sende Button");
            bericht = 0;
          } else {
            client.print(split(stat[bericht],0)); 
            client.print(":");
            client.print(split(stat[bericht],1)/10);
            client.print(split(stat[bericht],1)%10);
            client.print(" Uhr"); 
          }
        }
        
        if( grund == 1 || grund == 3 || grund == 6 ) client.print("Warnungs");
        if( grund == 2 || grund == 4 || grund == 7 ) client.print("Alarm");
        if( grund == 5 ) client.print("ATM Start");
        client.print(" Nachricht");
        client.println("</h2>");
  
        client.println("<div class='intext left'>");

        client.print("<div class='highlighted middle'>");
        
        if( grund == 0 ) {
          client.print("<span class='small'>Temperatur</span><br />");
          client.print( (int)temperaturC );
          client.print("&deg;C");
        }
        
        if( grund == 1 ) {
          client.print("<span class='small'>Temperatur</span><br />");
          client.print( (int)temperaturC - warnTemp );
          client.print("&deg;C");
          client.print("<br /><span class='small'>&uuml;berschritten</span>");
        }
        
        if( grund == 2 ) {
          client.print("<span class='small'>Temperatur</span><br />");
          client.print( (int)temperaturC - (warnTemp + alarmTemp) );
          client.print("&deg;C");
          client.print("<br /><span class='small'>&uuml;berschritten</span>");
        }
        
        if( grund == 3 ) {
          client.print("<span class='small'>Rel. Luftfeuchtigkeit</span><br />");
          client.print( (int)feuchte - warnWet );
          client.print("%");
          client.print("<br /><span class='small'>&uuml;berschritten</span>");
        }
        
        if( grund == 4 ) {
          client.print("<span class='small'>Rel. Luftfeuchtigkeit</span><br />");
          client.print( (int)feuchte - (warnWet + alarmWet) );
          client.print("%");
          client.print("<br /><span class='small'>&uuml;berschritten</span>");
        }
        
        if( grund == 5 ) {
          client.print("<span class='small'>Temperatur</span><br />");
          client.print( (int)temperaturC );
          client.print("&deg;C");
        }
        
        if( grund == 6 || grund == 7 ) {
          client.print("<span class='small'>F&uuml;llstand auf</span><br />");
          client.print( liter );
          client.print("<br /><span class='small'>Liter abgesunken</span>");
        }
       
        client.println("</div>");
        client.println("<br />\n");
        
        client.print("\n<hr>\n");
        client.println("<br />\n");
        
        client.print("Die aktuelle Temperatur ");
        client.print("im Terrarium betr&auml;gt gerade <b style='color;");
        if( temperaturC > 25 ) { client.print("AA0000"); } else { client.print("00AA00"); }
        client.print("'>");
        client.print(temperaturC);
        client.print("&deg;C</b>.");

        if( maxTempDay > 0 && grund != 5 ){
          client.println("<br /><br />\n\n");
          client.print("Die H&ouml;chsttemperatur lag bei <b>");
          client.print(maxTempDay);
          client.print("&deg;C</b>");
        }
        if( minTempDay > 0 && minTempDay != maxTempDay ){
          client.print(", die niedrigste gemessene Temperatur lag bei <b>");
          client.print(minTempDay);
          client.print("&deg;C</b>");
        }
        if( maxTempDay > 0 && grund != 5 ) client.print(". ");
        client.println("<br /><br />\n\n");
        
        
        // Google Chartimage Minutly Temperatur
        if( tempArrMCounter >= (sizeof(tempArrM)/sizeof(byte)) ) {
          
          mini = warnTempLow - 5;
          maxi = (alarmTemp + warnTemp) + 5;
          steps = 5;
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=ls");
          client.print("&chs=400x300");
          client.print("&chg=10,");
          client.print( 100 / ( ( ( maxi - mini ) / steps ) * 2.0 ) );
          client.print(",1.5");
          client.print("&chxl=3:|Minuten|2:|");
          client.print( warnTempLow );
          client.print("%20low|");
          client.print( warnTemp );
          client.print("%20warn|");
          client.print( (alarmTemp + warnTemp) );
          client.print("%20alarm");
          client.print("&chxp=2,");
          client.print( warnTempLow );
          client.print(",");
          client.print( warnTemp );
          client.print(",");
          client.print( (alarmTemp + warnTemp) );
          client.print("|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF2222");
          client.print("&chxt=x,y,r,x");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chma=40,140,30,30");
          client.print("&chdl=Temp%20%C2%B0C");
          client.print("&chtt=Temperatur");
          client.print("&chco=FF9900");
          client.print("&chds=");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("&chxr=1,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print(",");
          client.print( steps );
          client.print("|2,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("|0,-9,0,1");
          
          client.print("&chd=t:");
          for( i = 0; i < tempArrMSize; i++) {
            if(i != 0) client.print(",");
            client.print(tempArrM[i]);
          }

          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("10min Temperaturen Chart");
          client.print("' ");
          client.print("/>");
          
          client.println("\n");
          
        } 
       
        
        // Google Chartimage Hourly Temperatur
        if( tempArrCounter >= (sizeof(tempArr)/sizeof(byte)) ) {
          
          mini = warnTempLow - 5;
          maxi = (alarmTemp + warnTemp) + 5;
          steps = 5;
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=ls");
          client.print("&chs=400x300");
          client.print("&chg=10,");
          client.print( 100 / ( ( ( maxi - mini ) / steps ) * 2.0 ) );
          client.print(",1.5");
          client.print("&chxl=3:|Stunden|2:|");
          client.print( warnTempLow );
          client.print("%20low|");
          client.print( warnTemp );
          client.print("%20warn|");
          client.print( (alarmTemp + warnTemp) );
          client.print("%20alarm");
          client.print("&chxp=2,");
          client.print( warnTempLow ); 
          client.print(",");
          client.print( warnTemp );
          client.print(",");
          client.print( (alarmTemp + warnTemp) );
          client.print("|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF2222");
          client.print("&chxt=x,y,r,x");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chma=40,140,30,30");
          client.print("&chdl=Temp%20%C2%B0C");
          client.print("&chtt=Temperatur");
          client.print("&chco=FF9900");
          client.print("&chds=");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("&chxr=1,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print(",");
          client.print( steps );
          client.print("|2,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("|0,-9,0,1");
          
          client.print("&chd=t:");
          for( i = 0; i < tempArrSize; i++) {
            if(i != 0) client.print(",");
            client.print(tempArr[i]);
          }          
          
          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("10h Temperaturen Chart");
          client.print("' ");
          client.print("/>");
          
          client.println("\n");
          
        } 
        
        client.print("\n<hr>\n");
        client.println("<br />\n");
                
        client.print("Die Relative Luftfeuchtigkeit liegt bei <b>");
        client.print(feuchte);
        client.print("%</b>");
        client.print(". ");
                
        if( maxWetDay > 0  && grund != 5 ){
          client.println("<br /><br />\n\n");
          client.print("Sie erreichte maximal <b>");
          client.print(maxWetDay);
          client.print("%</b>");
        }
        if( minWetDay > 0 && minWetDay != maxWetDay ){
          client.print(" und der niedrigst gemessene Wert lag bei <b>");
          client.print(minWetDay);
          client.print("%</b>");
        }
        if( maxWetDay > 0 && grund != 5 ) client.print(". ");
        
        client.println("<br /><br />\n\n");
        
                
        // Google Chartimage Hourly Luftfeuchtigkeit
        if( wetArrCounter >= (sizeof(wetArr)/sizeof(byte)) ) {
          
          mini = warnWetLow - 5;
          maxi = (alarmWet + warnWet) + 5;
          steps = 5;
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=ls");
          client.print("&chs=400x300");
          client.print("&chg=10,");
          client.print( 100 / ( ( maxi - mini ) / steps ) );
          client.print(",1.5");
          client.print("&chxl=3:|Stunden|2:|");
          client.print( warnWetLow );
          client.print("%20low|");
          client.print( warnWet );
          client.print("%20warn|");
          client.print( (alarmWet + warnWet) );
          client.print("%20alarm");
          client.print("&chxp=2,");
          client.print( warnWetLow );
          client.print(",");
          client.print( warnWet );
          client.print(",");
          client.print( (alarmWet + warnWet) );
          client.print("|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF2222");
          client.print("&chxt=x,y,r,x");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chma=40,140,30,30");
          client.print("&chds=");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("&chxr=1,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print(",");
          client.print( steps );
          client.print("|2,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("|0,-9,0,1");
          
          client.print("&chdl=RF%20%");
          client.print("&chtt=Relative%20Luftfeuchtigkeit");
          client.print("&chco=008FCB");

          client.print("&chd=t:");
          for( i = 0; i < wetArrSize; i++) {
            if(i != 0) client.print(",");
            client.print( wetArr[i] );
          }
          
          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("10h Luftfeuchtigkeits Chart");
          client.print("' ");
          client.print("/>");

          client.println("\n");
          
        }
        
        
        client.print("\n<hr>\n");
        client.println("<br />\n");

        client.print("Der Regenwassertank ist noch zu <b>");
        client.print(liter,1);
        client.println("</b> Liter mit Wasser gef&uuml;llt");

        if( liter <= reFill && liter > (minFillDay / 10) ) {
          client.print(", es sollte langsam ans Nachf&uuml;llen gedacht werden!");
        } else {
          client.print(". ");
        }
                
        if( (minFillDay / 10) > 0  && grund != 5 ){
          client.println("<br /><br />\n\n");
          client.print("Der niedrigste erreichte F&uuml;llstand lag bei <b>");
          client.print( ( minFillDay / 10.0 ) , 1);
          client.print("</b> Liter. ");
        }
        
        client.println("<br /><br />\n\n");
                
        
        // Google Chartimage Hourly Füllstand
        if( fillArrCounter >= (sizeof(fillArr)/sizeof(byte)) ) {
          
          mini = 0;
          maxi = 20;
          steps = 2;
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=ls");
          client.print("&chs=400x300");
          client.print("&chg=10,");
          client.print( 100 / ( ( maxi - mini ) / ( steps * 1.0 ) ) );
          client.print(",1.5");
          client.print("&chxl=3:|Stunden|2:|");
          client.print( reFill );
          client.print("%20low|");
          client.print( warnFill );
          client.print("%20warn|");
          client.print( alarmFill );
          client.print("%20alarm");
          client.print("&chxp=2,");
          client.print( reFill );
          client.print(",");
          client.print( warnFill );
          client.print(",");
          client.print( alarmFill);
          client.print("|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF2222");
          client.print("&chxt=x,y,r,x");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chma=40,140,30,30");
          client.print("&chdl=Liter");
          client.print("&chtt=F%C3%BCllstand");
          client.print("&chco=0000FF");
          client.print("&chds=");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("&chxr=1,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print(",");
          client.print( steps );
          client.print("|2,");
          client.print( mini );
          client.print(",");
          client.print( maxi );
          client.print("|0,-9,0,1");
                    
          client.print("&chd=t:");
          for( i = 0; i < fillArrSize; i++) {
            if(i != 0) client.print(",");
            client.print( fillArr[i] / 10.0 );
          }
          
          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("10h Fuellstands Chart");
          client.print("' ");
          client.print("/>");
          
          client.println("\n");          
        }
        
        
        // einfacher Übersichtschart Temp, RF, Fill
                
        if( tempArrDCounter >= (sizeof(tempArrD)/sizeof(byte)) ) {
          
          client.print("\n<hr>\n");
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=ls");
          client.print("&chs=400x300");
          client.print("&chma=40,140,30,30");
          client.print("&chg=10,10,2");
          client.print("&chxr=0,-9,0,1");
          client.print("&chxt=x,y,r,x");
          client.print("&chxl=3:|Tage|2:|10%20min|90%20max");
          client.print("&chxp=2,10,90|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF0000");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chdl=Temp%20%C2%B0C|RL%20%|F%C3%BCllst.%20Liter");
          client.print("&chtt=Temperatur%20/%20Relative%20Luftfeuchte%20/%20F%C3%BCllstand");
          client.print("&chco=FF7300,008FCB,0000FF");
          client.print("&chd=t:");
          for( i = 0; i < tempArrDSize; i++) {
            if(i != 0) client.print("," );
            client.print( tempArrD[i] );
          }
          client.print("|");
          for( i = 0; i < wetArrDSize; i++) {
            if(i <= 8){
              if(i != 0) client.print(",");
              client.print( wetArrD[i] );
            }
          }       
          client.print("|");       
          for( i = 0; i < fillArrDSize; i++) {
            if(i <= 8){
              if(i != 0) client.print(",");
              client.print( fillArrD[i] / 10 );
            }
          }        
          
          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("9 Tage LinienChart");
          client.print("' ");
          client.print("/>");
          
          client.println("\n");         
        } 
        
        
        // Komplett Übersichtschart
                
        if( tempArrDCounter >= (sizeof(tempArrD)/sizeof(byte)) ) {
          
          client.print("\n<hr>\n");
          
          client.print("<img ");
          client.print("src='");
          
          client.print("https://chart.googleapis.com/chart?");
          client.print("cht=bvo");
          client.print("&chs=400x300");
          client.print("&chma=40,160,30,30");
          client.print("&chg=10,10,2");
          client.print("&chxr=0,-7,0,1");
          client.print("&chxt=x,y,r,x");
          client.print("&chxl=3:|Tage|2:|10%20min|90%20max");
          client.print("&chxp=2,10,90|3,50");
          client.print("&chxs=2,0000DD,13,-1,t,FF0000");
          client.print("&chxtc=1,10|2,-220");
          client.print("&chdl=Temp%20%C2%B0C|RL%20%|F%C3%BCllst.%20Liter");
          client.print("&chtt=Temperatur%20/%20Relative%20Luftfeuchte%20/%20F%C3%BCllstand");
          client.print("&chco=FF7300,008FCB,0000FF");
          client.print("&chd=t:");
          for( i = 0; i < tempArrDSize; i++) {
            if(i <= 7){
              if(i != 0) client.print(",");
              client.print( tempArrD[i] );
            }
          }
          client.print("|");
          for( i = 0; i < wetArrDSize; i++) {
            if(i <= 7){
              if(i != 0) client.print(",");
              client.print( wetArrD[i] );
            }
          }       
          client.print("|");       
          for( i = 0; i < fillArrDSize; i++) {
            if(i <= 7){
              if(i != 0) client.print(",");
              client.print( fillArrD[i] / 10 );
            }
          }
          
          client.print("' ");
          client.print("class='gchart' ");
          client.print("alt='");
          client.print("9 Tage BalkenChart");
          client.print("' ");
          client.print("/>");
          
          client.println("\n");
        }

        
        // Google Chart für Versand Gründe
                
        if( grundArrCounter >= (sizeof(grundArr)/sizeof(byte)) ) {
                    
          byte grundMeldung = 0;         // 0 Tagesbericht
          byte grundTempWarn = 0;        // 1 Temp Warn
          byte grundTempAlarm = 0;       // 2 Temp Alarm
          byte grundFeuchteWarn = 0;     // 3 Wet Warn
          byte grundFeuchteAlarm = 0;    // 4 Wet Alarm
          byte grundFuellstandWarn = 0;  // 6 Fill Warn
          byte grundFuellstandAlarm = 0; // 7 Fill Alarm
          
          for( i = 0; i < grundArrCounter; i++) {
            
            switch(grundArr[i]) {
              case 0:
                grundMeldung++;
                break;
              case 1:
                grundTempWarn++;
                break;
              case 2:
                grundTempAlarm++;
                break;
              case 3:
                grundFeuchteWarn++;
                break;
              case 4:
                grundFeuchteAlarm++;
                break;
              case 5:
                
                break;
              case 6:
                grundFuellstandWarn++;
                break;
              case 7:
                grundFuellstandAlarm++;
                break; 
              
            }
          
          } // ENDE for
          
          if( grundMeldung != 10 && grundTempWarn != 10 && grundTempAlarm != 10 && grundFeuchteWarn != 10 && grundFeuchteAlarm != 10 && grundFuellstandWarn != 10 && grundFuellstandAlarm != 10 ) {
                        
            client.print("\n<hr>\n");
          
            client.print("<img ");
            client.print("src='");
            
            client.print("https://chart.googleapis.com/chart?");
            client.print("cht=p");
            client.print("&chs=400x140");
            client.print("&chma=10,110,5,5");
            
            client.print("&chl=");
            if( grundFuellstandAlarm != 0 ) { client.print(grundFuellstandAlarm); client.print("%20F%C3%BCllAlarm"); }
            if( grundFuellstandWarn != 0 ) { if( grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundFuellstandWarn); client.print("%20F%C3%BCllWarn"); }
            if( grundFeuchteAlarm != 0 ) { if( grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundFeuchteAlarm); client.print("%20FeuchteAlarm"); }
            if( grundFeuchteWarn != 0 ) { if( grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundFeuchteWarn); client.print("%20FeuchteWarn"); }
            if( grundTempAlarm != 0 ) { if( grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundTempAlarm); client.print("%20TempAlarm"); }
            if( grundTempWarn != 0 ) { if( grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundTempWarn); client.print("%20TempWarn"); }
            if( grundMeldung != 0 ) { if( grundTempWarn != 0 || grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundMeldung); client.print("%20Meldung"); }
                     
            client.print("&chdl=");
            if( grundFuellstandAlarm != 0 ) { client.print(grundFuellstandAlarm); client.print("%20F%C3%BCllstandAlarm"); }
            if( grundFuellstandWarn != 0 ) { if( grundFuellstandAlarm != 0  ) { client.print("|"); } client.print(grundFuellstandWarn); client.print("%20F%C3%BCllstandWarn"); }
            if( grundFeuchteAlarm != 0 ) { if( grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundFeuchteAlarm); client.print("%20FeuchteAlarm"); }
            if( grundFeuchteWarn != 0 ) { if( grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundFeuchteWarn); client.print("%20FeuchteWarn"); }
            if( grundTempAlarm != 0 ) { if( grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundTempAlarm); client.print("%20TempAlarm"); }
            if( grundTempWarn != 0 ) { if( grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundTempWarn); client.print("%20TempWarn"); }
            if( grundMeldung != 0 ) { if( grundTempWarn != 0 || grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print(grundMeldung); client.print("%20Meldung"); }
            
            client.print("&chco=");
            if( grundFuellstandAlarm != 0 ) { client.print("008585"); }
            if( grundFuellstandWarn != 0 ) { if( grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("0000FF"); }
            if( grundFeuchteAlarm != 0 ) { if( grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("004ECB"); }
            if( grundFeuchteWarn != 0 ) { if( grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("008FCB"); }
            if( grundTempAlarm != 0 ) { if( grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("FF0000"); }
            if( grundTempWarn != 0 ) { if( grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("FF7300"); }
            if( grundMeldung != 0 ) { if( grundTempWarn != 0 || grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print("|"); } client.print("00800F"); }
         
            client.print("&chd=t:");
            if( grundFuellstandAlarm != 0 ) { client.print(grundFuellstandAlarm); }
            if( grundFuellstandWarn != 0 ) { if( grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundFuellstandWarn); }
            if( grundFeuchteAlarm != 0 ) { if( grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundFeuchteAlarm); }
            if( grundFeuchteWarn != 0 ) { if( grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundFeuchteWarn); }
            if( grundTempAlarm != 0 ) { if( grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundTempAlarm); }
            if( grundTempWarn != 0 ) { if( grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundTempWarn); }
            if( grundMeldung != 0 ) { if( grundTempWarn != 0 || grundTempAlarm != 0 || grundFeuchteWarn != 0 || grundFeuchteAlarm != 0 || grundFuellstandWarn != 0 || grundFuellstandAlarm != 0 ) { client.print(","); } client.print(grundMeldung); }
            
            client.print("' ");
            client.print("class='gchart' ");
            client.print("alt='");
            client.print("Versandgruende Chart");
            client.print("' ");
            client.print("/>");
            
            client.println("\n");
          }
        }
        
        client.print("\n<hr>");
        client.print("<br />\n\n"); 
        
        if( grund != 5 ) {
                           
          client.print("ATM wurde ");
          client.print(weekday(startDate[3]));
          client.print(", den <b>");
          client.print(startDate[0]);
          client.print(".");
          client.print(startDate[1]);
          client.print(".");
          client.print(startDate[2]);
          client.println("</b> um <b>"); 
          client.print(startDate[4]);
          client.print(":");
          client.print(startDate[5]/10);
          client.print(startDate[5]%10);
          client.println("</b> Uhr gestartet");

          client.print(" und l&auml;uft jetzt seit ");
          
          if( laufZeit[0] > 0 ) {
            client.print("<b>");
            client.print( laufZeit[0] );
            client.print("</b>");
            if( laufZeit[0] == 1 ) { client.print(" Woche"); } else { client.print(" Wochen"); }   
          }
          
          client.println();
          
          if( laufZeit[1] > 0 ) {
            if( laufZeit[0] > 0 ) client.print(", ");
            client.print("<b>");
            client.print( laufZeit[1] );
            client.print("</b>");
            if( laufZeit[1] == 1 ) { client.print(" Tag"); } else { client.print(" Tagen"); }  
          }
          
          client.println();
          
          if( laufZeit[2] > 0 ) {
            if( laufZeit[1] > 0 || laufZeit[0] > 0 ) client.print(", ");
            client.print("<b>");
            client.print( laufZeit[2] );
            client.print("</b>");
            if( laufZeit[2] == 1 ) { client.print(" Stunde"); } else { client.print(" Stunden"); } 
          }
          
          client.println();
          
          if( laufZeit[3] > 0 ) {
            if( laufZeit[2] > 0 || laufZeit[1] > 0 || laufZeit[0] > 0 ) client.print(" und ");
            client.print("<b>");
            client.print( laufZeit[3] );
            client.print("</b>");
            if( laufZeit[3] == 1 ) { client.print(" Minute"); } else { client.print(" Minuten"); }
          }
          
          client.println(". <br /><br />");
        }
              
        client.print("Der RAM verbrauch liegt ");
        client.print("aktuell bei <b>");
        client.print( availableMemory() );
        client.println("</b> Byte freiem Speicher");
        client.print(" und <b>");
        client.print( 8192 - availableMemory() );
        client.print("</b> Byte verwendeten SRAM Speicher.");
        
        client.println("<br /><br />\n\n");
        
        // geschaltete RelaisPins als kleine Übersicht
        for(i = 0; i < (sizeof(rel)/sizeof(byte)); i++) {
          if( relState[i] == HIGH ) {
            if( i == 0 ) { client.print("Relais #"); client.print( i + 1 ); client.print(" <b>Heizlampe</b>"); }
              else if( i == 1 ) { client.print("Relais #"); client.print( i + 1 ); client.print(" <b>Beregnungspumpe</b>"); }
              else if( i == 2 ) { client.print("Relais #"); client.print( i + 1 ); client.print(" <b>L&uuml;fter</b>"); }
              else if( i == 3 ) { client.print("Relais #"); client.print( i + 1 ); client.print(" <b>Energiesparlampe</b>"); }
              else { 
                client.print("Relais #"); 
                client.print( i + 1 );
                client.print(" - - - - - "); 
              }
            client.print(" ist Eingeschaltet ");
            client.println("<br/>");
            if( i == (sizeof(rel)/sizeof(byte)) ) client.println("<br/>");
          }
        }
        
        client.println("</div>"); // ENDE intext 
        
        client.println("<br /><br />\n\n");
        
        client.print("<div>Diese Mail wurde ");
        
        client.print(weekday(weekdayVal));
        client.print(", den <b>");
        client.println(day);
        client.print(".");
        client.print(month);
        client.print(".");
        client.print(year);
        client.println("</b> um <b>");
        client.print(hour);
        client.print(":");
        client.print(minute/10);
        client.print(minute%10);
        client.print(":");
        client.print(second/10);
        client.print(second%10);
        client.println("</b> Uhr versendet.");
        
        client.print("</div>");
                
        client.println("<br />\n");
        
        client.println("</div>"); // ENDE wrap
        
        client.println("<br /><br />\n\n");
        
        client.println("</body>");
        client.println("</html>");
        
        // Ende der Eingabe (mit einem Umbruch gefolgt von einem Punkt) 
        client.println(".");
        // Verbindung trennen
        client.println("QUIT");
        
        Serial.println();
        Serial.print("Sendstatus: ");
        Serial.print(grund);
        Serial.print(", erfolgreich versendet und Verbindung ");

        client.stop();
        Serial.println("getrennt ");
        
        // Reset some Values
        checked = false;
        versenden = false;
        
      } else {
        
        // if you didn't get a connection to the server:
        Serial.print("ERROR - No connection to ");
        Serial.println(domain);
        
        // Error
        errCode = 7; // 7 = Sending fail

        checked = true;
    } // ENDE client.connected
    
  } else { // ENDE client.connect
    // Error
    errCode = 6; // 6 Connection to server failed
  }
  return checked;
} // ENDE Methode mailSenden()


// Herzsymboanzeige (Heartbeat)
void heart() {
  lcd.setCursor(15,0);
  if(hbeat) {
    lcd.print("-");
    hbeat = false;
  } else {
    lcd.print("*");
    hbeat = true;
  }
}


// Sekunden Punkte 
void dots(int row, int colum) {
  if( second % 2 == 0 ) {
    lcd.setCursor(row,colum);
    lcd.write(":");
  } else {
    lcd.setCursor(row,colum);
    lcd.write(" ");
  }
}


// Integer joiner
int join( int hours, int minutes ){
  return (  
      ((hours / 10 ) * 1000) +                 // tausender
      ((hours - (hours / 10) * 10) * 100) +    // hunderter
      (( minutes / 10 ) * 10) +                // zehner
      (minutes - (minutes / 10) * 10)          // einer
      );
}


// Integer splitter
// returns (returnVal: 0 first or 1 last) 2 letterpairs of 4 given
int split( int wert, int returnVar ){ 
  
  if( wert < 99999 || returnVar > 1 ){
    int einer, zehner, hunderter, tausender, zehntausender;
    zehntausender = wert / 10000 * 10000;
    tausender = (wert - zehntausender) / 1000 * 1000;
    hunderter = (wert - tausender) / 100 * 100;
    zehner = ( wert - (tausender + hunderter) ) / 10 * 10;
    einer = ( wert - ( tausender + hunderter + zehner) );
    
    if( returnVar == 0 ) { // returns default the first
      return ( (zehntausender / 10000 * 1000) + ( tausender / 1000 * 10 ) + (hunderter / 100)   );
    } else {
      return (zehner + einer);
    }
  } else {
    return 0;
  }
  
  
}

// Fuellstandsanzeige
float fill(int TriggerPin,int EchoPin) {
  pinMode(TriggerPin, OUTPUT);
  digitalWrite(TriggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TriggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TriggerPin, LOW);
  pinMode(EchoPin, INPUT);
  return pulseIn(EchoPin, HIGH) / 29.0 / 2.0;
}

// mapfloat, mapping mit floatvariablen
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Deutsche Wochentage (forms: 0 kurz, 1 lang)
String weekday( int day ){
  String namedDay;
  switch(day){
    case 1: namedDay = "Mo"; break;
    case 2: namedDay = "Di"; break;
    case 3: namedDay = "Mi"; break;
    case 4: namedDay = "Do"; break;
    case 5: namedDay = "Fr"; break;
    case 6: namedDay = "Sa"; break;
    case 7: namedDay = "So"; break;
  }
  return namedDay;
}

// Deutsche Monate (forms: 0 kurz, 1 lang)
String monthName( int monthVal ){
  String namedMonth;
  switch(monthVal){
    case  1: namedMonth = "Jan"; break;
    case  2: namedMonth = "Feb"; break;
    case  3: namedMonth = "Mae"; break;
    case  4: namedMonth = "Apr"; break;
    case  5: namedMonth = "Mai"; break;
    case  6: namedMonth = "Jun"; break;
    case  7: namedMonth = "Jul"; break;
    case  8: namedMonth = "Aug"; break;
    case  9: namedMonth = "Sep"; break;
    case 10: namedMonth = "Okt"; break;
    case 11: namedMonth = "Nov"; break;
    case 12: namedMonth = "Dez"; break;
  }
  return namedMonth;
}

int availableMemory() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


// Statusanzeige
void showStat(){
  Serial.print(weekday(weekdayVal));
  Serial.print(" ");
  Serial.print(day);
  Serial.print(".");
  Serial.print(monthName(month));
  Serial.print(".");
  Serial.print(split(year,1));
  Serial.print(" ");
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute/10);
  Serial.print(minute%10);
  Serial.print(":");
  Serial.print(second/10);
  Serial.print(second%10);
  Serial.print("  ");     // Spacer
  Serial.print("S:");
  Serial.print(versenden);
  Serial.print("|G:");
  Serial.print(grund);
  Serial.print("|L:");
  Serial.print(logging);
  Serial.print("  ");     // Spacer
  Serial.print((int)temperaturC);
  Serial.print(char(176));
  Serial.print("C|");
  Serial.print((int)feuchte);
  Serial.print("%|");
  Serial.print(liter);
  Serial.print("l");
  Serial.print("  ");     // Spacer
  Serial.print("M");
  Serial.print(tempArrMCounter);
  Serial.print("|H");
  Serial.print(tempArrCounter);
  Serial.print("|D");
  Serial.print(tempArrDCounter);
  Serial.print("  ");     // Spacer
  Serial.print(laufZeit[2]/10);
  Serial.print(laufZeit[2]%10);
  Serial.print(":");
  Serial.print(laufZeit[3]/10);
  Serial.print(laufZeit[3]%10);
  Serial.print("  ");     // Spacer
  Serial.print(availableMemory());
  Serial.print("/");
  Serial.print(8192-availableMemory());
  
  // Print errors to Serial
  if(errCode > 0){ Serial.print("ERR: "); Serial.print(errCode); }
  
  Serial.println();
}

void showStatLCD(){
    
  // Main Display 0 View
  if( displayVar == 0 ) {
    
    // 1st Line
  
    lcd.setCursor(0 ,0);
    lcd.print((int)temperaturC/10);
    lcd.print((int)temperaturC%10);
    lcd.print(char(223));
    lcd.print(" ");
    
    lcd.print( (int)feuchte/10 );
    lcd.print( (int)feuchte%10 );
    lcd.print("% ");
    
    if( (int)liter > 0 ) {
      if( (int)liter > 9 ){
        lcd.setCursor(8 ,0);
      } else {
        lcd.setCursor(8 ,0);
        lcd.print(" ");
      }
      lcd.print((int)liter);
    } else { // liter unter 1
      if( ( (liter * 100) / 10 ) > 9 ){
        lcd.setCursor(8 ,0);
      } else {
        lcd.setCursor(8 ,0);
      }
      lcd.print(",");
      lcd.print( (int)( (liter * 100) / 10) );
    }
    lcd.print("l ");
   
    // Sendstat and Errorview
    lcd.setCursor(13 ,0);
    // Print errors if there are to the LCD
    if(errCode > 0){
  //    lcd.setCursor(14 ,0);
  //    lcd.print(errCode);
    } else {
      // NoError 
      if( versenden && ( alarmIntervalCount <= 1 && warnIntervalCount <= 1 ) ){
        lcd.print(grund); // Or View SendCode
      } else {
        lcd.write(3); // View lettersmbol if sending is false
      }
    }
    
    // 2nd Line
    
    if(errCode == 1){
      lcd.setCursor(0 ,1);
      lcd.print(" RTC Time fail! ");
    } else {
      if( hour > 9 ){
        lcd.setCursor(0 ,1);
      } else {
        lcd.setCursor(0 ,1);
        lcd.print(" ");
      }
      lcd.print(hour);
      lcd.setCursor(3 ,1);
      lcd.print(minute/10);
      lcd.print(minute%10);
      lcd.print(" ");
      lcd.setCursor(6 ,1);
      lcd.print(weekday(weekdayVal));
      lcd.print(" ");
      lcd.setCursor(9 ,1);
      if( day < 10 ) lcd.print(" ");
      lcd.print(day);
      lcd.print(monthName(month));
      lcd.print(split(year,1));
    }
    
  } 
  
  // Present Display 1 View
  if( displayVar == 1 ){
    
    // 1st Line
    
    // View Reporttimes 
    for(i = 0; i < (sizeof(stat)/sizeof(unsigned int)); i++) {
      if( second > ( i * ( 60 / (sizeof(stat)/sizeof(unsigned int)) ) ) ) {
        lcd.setCursor(0 ,0);
        lcd.print("Rep#");
        lcd.print( i + 1 );
        lcd.print(" ");
        if(split(stat[i],0) < 10) lcd.print(" ");
        lcd.print(split(stat[i],0));
        lcd.print(":");
        lcd.print(split(stat[i],1)/10);
        lcd.print(split(stat[i],1)%10);
        lcd.print(" ");
      } else {
        break;
      }
    }

    
    // Sendstatus und FehlerCounterAnzeige
    lcd.setCursor(13 ,0);
    // Print errors if there are to the LCD
    if(errCode > 0){
      lcd.setCursor(14 ,0);
      lcd.print(errCode);
    } else {
      // NoError 
      if(versenden && ( alarmIntervalCount <= 1 && warnIntervalCount <= 1 ) ){
        lcd.print(grund); // Or View SendCode
      } else {
        lcd.write(3); // View lettersmbol if sending is false
      }
    }
    
    // 2nd Line
    
    lcd.setCursor(0 ,1);
 
    // Tage
    if( ( laufZeit[1] + ( laufZeit[0] * 7 ) )  > 0 ) {
      if( ( laufZeit[1] + ( laufZeit[0] * 7 ) ) < 100 ) lcd.print(" ");
      if( ( laufZeit[1] + ( laufZeit[0] * 7 ) ) < 10 ) lcd.print(" ");
      lcd.print( ( laufZeit[1] + ( laufZeit[0] * 7 ) ) );
    } else {
      lcd.print("   ");
    }
    
    lcd.print(" ");
    lcd.setCursor(4 ,1);
    
    // Stunden
    if( laufZeit[2] < 10 ) lcd.print(" ");
    lcd.print( laufZeit[2] );
    
    lcd.setCursor(7 ,1);
    
    // Minuten
    lcd.print( laufZeit[3]/10 );
    lcd.print( laufZeit[3]%10 );
    lcd.print("    ");
    
        
    lcd.setCursor(10 ,1);
    if( loggCounter > 0 ) { lcd.print("l"); lcd.print(loggCounter); } 
    if( sendCounter > 0 ) { lcd.print("s"); lcd.print(sendCounter); } 
    if( warnIntervalCount > 0 ) { lcd.print("w"); lcd.print(warnIntervalCount); } 
    if( alarmIntervalCount > 0 ) { lcd.print("a"); lcd.print(alarmIntervalCount); } 
    
    lcd.setCursor(13 ,1);
    lcd.print(availableMemory());
    
  }
  
  // Present Display 2 View
  if( displayVar == 2 ) {
    
    // Temperatur Werte Anzeige
    lcd.setCursor(0 ,0);
    lcd.write(5); // Temp
    lcd.print(" Temperatur    ");
    
    if(second > 30) {
    
    lcd.setCursor(0 ,1);
    lcd.print("min ");
    lcd.print(minTempDay);
    lcd.print(char(223));
    lcd.print("C");
    
    lcd.setCursor(8 ,1);
    lcd.print("max ");
    lcd.print(maxTempDay);
    lcd.print(char(223));
    lcd.print("C ");
    
    } else {
      
      lcd.setCursor(0 ,1);
      lcd.print("aktuell: ");
      lcd.setCursor(9 ,1);
      lcd.print((int)temperaturC/10);
      lcd.print((int)temperaturC%10);
      lcd.print(char(223));
      lcd.print("C   ");
      
    } 
    
  }
  
  // Present Display 3 View
  if( displayVar == 3 ){
    
    // Luftfeuchtigkeit Werte Anzeige
    lcd.setCursor(0 ,0);
    lcd.write(6); // Wet
    lcd.print(" Rel.Luftfeucht");
    
    if(second > 30) {
    
      lcd.setCursor(0 ,1);
      lcd.print("min ");
      if( minWetDay < 10 ) lcd.print(" ");
      lcd.print(minWetDay);
      lcd.print("% ");
      
      lcd.setCursor(8 ,1);
      lcd.print("max");
      if( maxWetDay < 100 ) lcd.print(" ");
      if( maxWetDay < 10 ) lcd.print(" ");
      lcd.print(maxWetDay);
      lcd.print("% ");
      
    } else {
      
      lcd.setCursor(0 ,1);
      lcd.print("aktuell: ");
      lcd.setCursor(9 ,1);
      lcd.print( (int)feuchte/10 );
      lcd.print( (int)feuchte%10 );
      lcd.print("%    ");
            
    }
  }
  
  
  if( displayVar == 4 ){
    
    // Füllstand Werte Anzeige
    lcd.setCursor(0 ,0);
    lcd.write(4); // Fill
    lcd.print(" F");
    lcd.print(char(245));
    lcd.print("llstand     ");
    
    if(second > 30) {
    
      lcd.setCursor(0 ,1);
      lcd.print("min");
      if( ( (minFillDay/10) ) < 10 ) lcd.print(" ");
      lcd.print( ( minFillDay / 10.0 ) );
      lcd.setCursor(7 ,1); // 
      lcd.print("l");
      
      lcd.setCursor(8 ,1);
      lcd.print("max");
      if( ( maxFillDay / 10 ) < 10 ) lcd.print(" ");
      lcd.print( ( maxFillDay / 10.0 ) );
      lcd.setCursor(15 ,1);
      lcd.print("l ");
      
    } else {
      
      lcd.setCursor(0 ,1);
      lcd.print("aktuell:  ");
      lcd.print( liter );
      lcd.print("l    ");

    }
    
  } 
  
  
  // Heizlampe Display 5 View
  if( displayVar == 5 ){
    
    lcd.setCursor(0 ,0);
    lcd.print("Heizlampe ");
    lcd.setCursor(11,0);
    lcd.print(" Rel1");
    
    if( relState[0] ){
      lcd.setCursor(10,0); lcd.write(8);
    } else {
      lcd.setCursor(10,0); lcd.write(" ");
    }
    
    lcd.setCursor(0 ,1);
    lcd.print("G");
    lcd.print(heat[0]); // start h
    lcd.print("h S");
    lcd.print(heat[1]); // stop  h
    lcd.print("h T");
    lcd.print(heat[2]); // maxTemp
    lcd.print(char(223));
    lcd.print("C   ");
    
  }
  
  
  // Beregnungs Display 6 View
  if( displayVar == 6 ){
    
    lcd.setCursor(0 ,0);
    lcd.print("Beregnung ");
    lcd.setCursor(11,0);
    lcd.print(" Rel2");
    
    if( relState[1] ){
      lcd.setCursor(10,0); lcd.write(6);
    } else {
      lcd.setCursor(10,0); lcd.write(" ");
    }
        
    if( second > 30 ) {
      lcd.setCursor(0 ,1);
      lcd.print("  Di Mi Do Fr   ");
    } else {
      lcd.setCursor(0 ,1);
      lcd.print("G");
      lcd.print(rain[0]); // start m
      lcd.print("m S");
      lcd.print(rain[1]); // stop  m
      lcd.print("m H");
      lcd.print(rain[2]); // Stunde
      lcd.print("h    ");
    }    
  }
  
  // Lüfter Display 7 View
  if( displayVar == 7 ){
    
    lcd.setCursor(0 ,0);
    lcd.print("L");
    lcd.print(char(245));
    lcd.print("fter    ");
    lcd.setCursor(11,0);
    lcd.print(" Rel3");
    
    if( relState[2] ){
      lcd.setCursor(10,0); lcd.write(1);
    } else {
      lcd.setCursor(10,0); lcd.write(" ");
    }
    
    lcd.setCursor(0 ,1);
    lcd.print("Auto bei ");
    lcd.print(( warnTemp - 3 ));
    lcd.print(char(223));
    lcd.print("C   ");
    
  }
  
  // Energiesparlampen Display 8 View
  if( displayVar == 8 ){
    
    lcd.setCursor(0 ,0);
    lcd.print("E-Spar L. ");
    lcd.setCursor(11,0);
    lcd.print(" Rel4");
    
    if( relState[3] ){
      lcd.setCursor(10,0); lcd.write(2);
    } else {
      lcd.setCursor(10,0); lcd.write(" ");
    }
    
    lcd.setCursor(0 ,1);
    lcd.print("G");
    lcd.print(light[0]); // start h
    lcd.print("h S");
    lcd.print(light[1]); // stop  h
    lcd.print("h T");
    lcd.print(light[2]); // maxTemp
    lcd.print(char(223));
    lcd.print("C   ");
    
    
  }
  
  // IP, Arrays and Copyright Display 9 View
  if( displayVar == 9 ){
    
    lcd.setCursor(0 ,0);
    lcd.print("ArduinoTempMailD");
    
    if(second < 20) {
            
      lcd.setCursor(0 ,1);
      lcd.print(" ");
      lcd.print(Ethernet.localIP());
      lcd.print("  ");      
      
    } else if(second < 40) {
      
      lcd.setCursor(0 ,1);
      lcd.print(" by ");
      lcd.print(benutzer);
      lcd.print("      ");
      
    } else {
        
      lcd.setCursor(0 ,1);
      
      lcd.print(" G");
      if(grundArrCounter < 10) lcd.print(" ");
      lcd.print(grundArrCounter);
      lcd.print(" M");
      if(tempArrMCounter < 10) lcd.print(" "); 
      lcd.print(tempArrMCounter);
      lcd.print(" H");
      if(tempArrCounter < 10) lcd.print(" ");
      lcd.print(tempArrCounter);
      lcd.print(" D");
      lcd.print(tempArrDCounter);
      lcd.print(" ");
       
    }
    
  }
   
  
  if( displayVar < 0 ) displayVar = 9;
  if( displayVar > 9 ) displayVar = 0;
  
} // Ende showStatLCD



// read the buttons 
int read_LCD_buttons() 
{  
  adc_key_in = analogRead(0);      // read the value from the sensor   
  // my buttons when read are centered at these valies: 0, 144, 329, 504, 741  
  // we add approx 50 to those values and check to see if we are close  
  if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result  
  if (adc_key_in < 50)   return btnRIGHT;    
  if (adc_key_in < 195)  return btnUP;   
  if (adc_key_in < 380)  return btnDOWN;  
  if (adc_key_in < 555)  return btnLEFT;   
  if (adc_key_in < 790)  return btnSELECT;    
  return btnNONE;  // when all others fail, return this... 
} 


void checkButtons() {
  lcd_key = read_LCD_buttons();  // read the buttons 
  
  switch (lcd_key)               // depending on which button was pushed, we perform an action  
  { 
  case btnNONE:
    {
      return;
    }

  case btnRIGHT:      
    {       
      // Send Mail Button
      if( !versenden && displayVar == 1 ) {
        versenden = true;
        grund = 0;
        bericht = 99;
      }
      
      if( displayVar == 5 ) {
        // Heizlampe ein/aus Wechselschalter
        if( relState[0] ){
          relState[0] = LOW;
          digitalWrite(rel[0], relState[0]);
        } else {
          relState[0] = HIGH;
          digitalWrite(rel[0], relState[0]);
        } 
      }
      
      if( displayVar == 6 ) {
        // Beregnung ein/aus Wechselschalter
        if( relState[1] ){
          relState[1] = LOW;
          digitalWrite(rel[1], relState[1]);
        } else {
          relState[1] = HIGH;
          digitalWrite(rel[1], relState[1]);
        }         
      }
      
      if( displayVar == 7 ) {
        // Beregnung ein/aus Wechselschalter
        if( relState[2] ){
          relState[2] = LOW;
          digitalWrite(rel[2], relState[2]);
        } else {
          relState[2] = HIGH;
          digitalWrite(rel[2], relState[2]);
        }         
      }
      
      if( displayVar == 8 ) {
        // Beregnung ein/aus Wechselschalter
        if( relState[3] ){
          relState[3] = LOW;
          digitalWrite(rel[3], relState[3]);
        } else {
          relState[3] = HIGH;
          digitalWrite(rel[3], relState[3]);
        }         
      }
      
      delay(1000);
      break;      
    }   
  case btnLEFT:      
    {   
      if( displayVar < 2 ) {
        // Turn Backlight on or off
        if(backlight) { 
          digitalWrite(LCD_LIGHT_PIN, LOW);
          backlight = false; 
        } else { 
          digitalWrite(LCD_LIGHT_PIN, HIGH);
          backlight = true; 
        }
      }
      delay(1000);
      break;      
    }   
  case btnSELECT:      
    { 
      displayVar = 0;
      delay(1000);
      break;      
    }   
  case btnDOWN:      
    {     
      // Set down
      displayVar--;
      delay(1000);
      break;      
    }   
  case btnUP:      
    {     
      // Set up
      displayVar++;
      delay(1000);
      break;      
    } 
  }
  
}

