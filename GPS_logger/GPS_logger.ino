#include <Adafruit_GPS.h>
#include <DHT.h>
#include <SD.h>
#include <SPI.h>

Adafruit_GPS GPS(&Serial1);

#define cardSelect 4
#define GREENLEDPIN 8
#define REDLEDPIN 13

#define VBATPIN A7
//#define DEBUG

#define DHTPIN 6
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

File logfile;

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  false
/* set to true to only log to SD when GPS has a fix, for debugging, keep it false */
#define LOG_FIXONLY false  


void setup() {
  pinMode(GREENLEDPIN, OUTPUT);
  digitalWrite(GREENLEDPIN, LOW);
  pinMode(REDLEDPIN, OUTPUT);
  digitalWrite(REDLEDPIN, LOW);

  // Serial.begin(115200);
  delay(1000);
  // Serial.println("Adafruit GPS logger!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data for high update rates!
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // uncomment this line to turn on all the available data - for 9600 baud you'll want 1 Hz rate
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_ALLDATA);
  
  // Set the update rate
  // Note you must send both commands below to change both the output rate (how often the position
  // is written to the serial line), and the position fix rate.
  // 1 Hz update rate
  //GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  //GPS.sendCommand(PMTK_API_SET_FIX_CTL_1HZ);
  // 5 Hz update rate- for 9600 baud you'll have to set the output to RMC or RMCGGA only (see above)
  //  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_5HZ);
  //  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);
  // 10 Hz update rate - for 9600 baud you'll have to set the output to RMC only (see above)
  // Note the position can only be updated at most 5 times a second so it will lag behind serial output.
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_10HZ);
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_5HZ);

  // Request updates on antenna status, comment out to keep quiet
  // GPS.sendCommand(PGCMD_ANTENNA);
  // Turn off updates on antenna status, if the firmware permits it
  GPS.sendCommand(PGCMD_NOANTENNA);

  delay(1000);

  // see if the card is present and can be initialized:
  if (!SD.begin(cardSelect)) {
    // Serial.println("Card init. failed!");
    error(2);
    }
  
  char filename[15];
  strcpy(filename, "GPSLOG00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    // Serial.print("Couldnt create "); Serial.println(filename);
    error(3);
  }
  // Serial.print("Writing to "); Serial.println(filename);
  
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
//  logfile.write("VBat: ");
//  logfile.write(measuredvbat,4);
//  logfile.flush();
  digitalWrite(GREENLEDPIN, HIGH);
  logfile.print("VBAT: "); logfile.println(measuredvbat, 4);
  logfile.flush();
  // Serial.print("VBAT: "); Serial.println(measuredvbat, 4);
  digitalWrite(GREENLEDPIN, LOW);
  
  delay(100);

  dht.begin();
  delay(2000);

  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    // Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  digitalWrite(GREENLEDPIN, HIGH);
  logfile.print("Humidity: "); logfile.print(h); logfile.println(" %");
  logfile.print("Temperature: "); logfile.print(t); logfile.println(" *C");
  logfile.print("Heat Index: "); logfile.print(hic); logfile.println(" *C");
  logfile.flush();
  digitalWrite(GREENLEDPIN, LOW);

//  Serial.print("Humidity: ");
//  Serial.print(h);
//  Serial.println(" %\t");
//  Serial.print("Temperature: ");
//  Serial.print(t);
//  Serial.println(" *C");
//  Serial.print("Heat index: ");
//  Serial.print(hic);
//  Serial.println(" *C ");
}

void error(uint8_t errno) {
  uint8_t i;
  for (i=0; i<errno; i++) {
    digitalWrite(REDLEDPIN, HIGH);
    delay(100);
    digitalWrite(REDLEDPIN, LOW);
    delay(100);
  }
  for (i=errno; i<10; i++) {
    delay(200);
  }
}

void loop()                     // run over and over again
{
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  
// if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    // Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
    
    if (LOG_FIXONLY && !GPS.fix) {
      //Serial.print("No Fix");
      return;
    }
    
    char *stringptr = GPS.lastNMEA();
    // int stringsize = strlen(stringptr);
    uint8_t stringsize = strlen(stringptr);
    //if (stringsize != logfile.write((uint8_t *)stringptr, stringsize))    //write the string to the SD file
    digitalWrite(GREENLEDPIN,HIGH);
    logfile.write((uint8_t *)stringptr, stringsize);    //write the string to the SD file
    digitalWrite(GREENLEDPIN,LOW);
    //if (stringsize != logfile.write("THE WRITING OF THE INFO"))    //write the string to the SD file
    if (strstr(stringptr, "RMC")) logfile.flush();
  }
}
