// libs for lux sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

// libs for clock
#include <Wire.h>
#include <DS3231.h>

// libs for sd card
#include <SPI.h>
#include <SD.h>
#define cardSelect 4
const String DELIM = ",";

// variables
// lux sensor
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

// clock
DS3231 clock;
RTCDateTime dt;

// sd card
File logfile;

// switch
const int switchPin = 11;
int switchState;

// led
const int ledPin = 13;

// data logging frequency
const int interval = 1 * 60 * 1000; // in msec

// battery voltage monitoring pin
#define VBATPIN A7

/**************************************************************************/
/*
    Displays some basic information on the lux sensor from the unified
    sensor API sensor_t type (see Adafruit_Sensor for more information)
*/
/**************************************************************************/
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2561
*/
/**************************************************************************/
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  //tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */

  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}

String formatDateTime(RTCDateTime dt) {
  String result = "";
  result += String(dt.year); result += "-";
  result += String(dt.month); result += "-";
  result += String(dt.day); result += DELIM;
  result += String(dt.hour); result += ":";
  result += String(dt.minute); result += ":";
  result += String(dt.second);

  return result;
}

// blink out an error code
void error(uint8_t errno) {
  while (1) {
    uint8_t i;
    // blink errno times
    for (i = 0; i < errno; i++) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
    // delay for 5s
    delay(5000);
  }
}

/**
   Read battery voltage.
   Board: Adafruit Feather M0, Feather 32u4
*/
float getBatteryVoltage() {
  return analogRead(VBATPIN) * 2 * 3.3 / 1024;
  // 2: voltage measured at VBATPIN is one half of the battery pin
  // 3.3: reference voltage
  // 1024: convert analogRead value to voltage
}

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/
void setup(void)
{
  // connect at 115200 so we can read the the data fast enough and echo without dropping chars
  Serial.begin(115200);

  // configure led
  pinMode(ledPin, OUTPUT);

  // configure switch
  pinMode(switchPin, INPUT);

  // configure battery pin
  pinMode(VBATPIN, INPUT);

  // if the two positive rails are not connected prior to powering the board
  // this tell the board not to do measuring but monitoring the battery voltage and signal via the led blink frequency
  // high frequency (10Hz) charging
  // low frequency (0.25 Hz): fully charged
  if (digitalRead(switchPin) == LOW) {
    // if the batter is not fully charged, blink fast
    while (1) {
      float batVolt = getBatteryVoltage();
      Serial.println();
      if (batVolt < 4.2) {
        digitalWrite(ledPin, HIGH);
        delay(100);
        digitalWrite(ledPin, LOW);
        delay(100);
      } else { // blink slow if the battery is fully charged
        digitalWrite(ledPin, HIGH);
        delay(4000);
        digitalWrite(ledPin, LOW);
        delay(4000);
      }
    }
  }
  else {
    Serial.println("jumper wire correctly connected");
    digitalWrite(ledPin, LOW);
  }

  ////////
  // configure light sensor
  ////////
  /* Initialise the sensor */
  //use tsl.begin() to default to Wire,
  //tsl.begin(&Wire2) directs api to use Wire2, etc.
  if (!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("No TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  /* Display some basic information on this sensor */
  displaySensorDetails();

  /* Setup the sensor gain and integration time */
  configureSensor();
  Serial.println("Lux sensor ready.");

  /////////
  // configure clock
  /////////
  Serial.println("Initialize DS3231");;
  clock.begin();
  // Set sketch compiling time
  //clock.setDateTime(__DATE__, __TIME__); // this should only be run when conneceted to a computer
  //Serial.println("Clock ready.");

  /////////
  // configure data logger
  /////////
  // see if the card is present and can be initialized:
  if (!SD.begin(cardSelect)) {
    Serial.println("Card init. failed!");
    error(2);
  }

  // set an incremental file name
  //dt = clock.getDateTime();
  //String fileName= "b";
  //fileName += String(dt.year);
  //fileName += String(dt.month);
  //fileName += String(dt.day);
  //fileName += "-";
  //fileName += String(dt.hour);
  //fileName += String(dt.minute);
  //fileName += String(dt.second);
  //fileName += ".log";

  char filename[15];
  strcpy(filename, "Y00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[1] = '0' + i / 10;
    filename[2] = '0' + i % 10;
    // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) {
          break;
        }
  }

  Serial.print("Log file: ");
  Serial.println(filename);

  // open file
  logfile = SD.open(filename, FILE_WRITE);
  if ( ! logfile ) {
    Serial.print("Couldnt create ");
    Serial.println(filename);
    error(3);
  }
  Serial.print("Writing to ");
  Serial.println(filename);

  pinMode(13, OUTPUT);
  pinMode(8, OUTPUT);
  Serial.println("SD card ready.");

  ///////////
  // Finishing the setup step and wait for the jumper wire to be disconnected
  ///////////
  // indicate setup completed
  digitalWrite(ledPin, HIGH);
  Serial.println("Setup completed");

  // loop until the positive rails are disconnected
  while (digitalRead(switchPin) == HIGH) {
  }

  digitalWrite(ledPin, LOW);
}

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void)
{
  /* Get a new sensor event */
  sensors_event_t event;
  tsl.getEvent(&event);

  /* Display the results (light is measured in lux) */
  if (event.light)
  {
    uint16_t broadband = 0; uint16_t infrared = 0;
    tsl.getLuminosity (&broadband, &infrared);
    
    dt = clock.getDateTime();
    Serial.print(formatDateTime(dt));
    Serial.print(DELIM);
    Serial.print(event.light); 
    Serial.print(DELIM);
    Serial.print(broadband); 
    Serial.print(DELIM);
    Serial.print(infrared); 
    Serial.println("");

    digitalWrite(8, HIGH);
    logfile.print(formatDateTime(dt));
    logfile.print(DELIM);
    logfile.print(event.light); 
    logfile.print(DELIM);
    logfile.print(broadband); 
    logfile.print(DELIM);
    logfile.print(infrared);
    logfile.println("");
    // logging frequency is quite low, flushing every data point may be a good idea
    logfile.flush();
    digitalWrite(8, LOW);
  }
  else
  {
    /* If event.light = 0 lux the sensor is probably saturated
       and no reliable data could be generated! */
    Serial.println("Sensor overload");
  }

  /////////
  // Check if the stop signal is received (jumper wire connected)
  /////////

  for (int i = 0; i < interval / 5000; i++) {

    if (digitalRead(switchPin) == HIGH) {
      Serial.println("wrapping things up");
      // wrap things up

      // logging frequency is quite low, flushing every data point may be a good idea
      logfile.flush();

      // turn on LED
      digitalWrite(ledPin, HIGH);
      Serial.println("Ready for disconnection");

      // do nothing - ready for disconnection
      while (1) {}
    }
    delay(5000);

  }
}
