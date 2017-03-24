#include <CurieBLE.h>
#include <Adafruit_Si7021.h>
/**
 * Bird Feeder sketch 2/14/2017
 */

/**
* Setup our BLE service and characteristics here
*/
BLEPeripheral blePeripheral;       // BLE Peripheral Device (the board you're programming)
BLEService birdFeederService("917649A0-D98E-11E5-9EEC-0002A5D5C51B"); // Custom UUID
BLEUnsignedCharCharacteristic perchCharacteristic("917649A1-D98E-11E5-9EEC-0002A5D5C51B", BLERead | BLENotify);
BLECharacteristic tempCharacteristic("917649A2-D98E-11E5-9EEC-0002A5D5C51B", BLERead | BLENotify, 4);
BLECharacteristic humidityCharacteristic("917649A3-D98E-11E5-9EEC-0002A5D5C51B", BLERead | BLENotify, 4);
BLECharacteristic feedCharacteristic("917649A4-D98E-11E5-9EEC-0002A5D5C51B", BLERead | BLENotify, 2);
BLEDescriptor birdFeederDescriptor("2902", "ble");


#define BLEPIN 4 // used for testing BLE connections,
#define LEFT_PERCH 8
#define RIGHT_PERCH 10

/**
* The union directive allows these variables to share the same memory location. Please see the
* my tutorial Imu to You!(https://www.hackster.io/gov/imu-to-you-ae53e1?team=18045) tutorial for further
* discussion of the use of the union directive in C.
*/
 union
 {
  float t;
  unsigned char bytes[4];
 } tempData;

 union
 {
  float h;
  unsigned char bytes[4];
 } humidityData;

 union
 {
  int f;
  unsigned char bytes[2];
 } feedData;

class BirdFeeder {

  long perchInterval = 250;
  long weatherInterval = 10000; // for testing purposes, changeto 30000(5 mins) or greater for initial deployment

  long currentPerchMillis, previousPerchMillis;
  int currentLeftPerch, currentRightPerch, previousLeftPerch, previousRightPerch;

  long currentWeatherMillis, previousWeatherMillis;

  public:

  Adafruit_Si7021 sensor;

  BirdFeeder(int dummy)
  {
    pinMode(LEFT_PERCH, INPUT);
    pinMode(RIGHT_PERCH, INPUT);
    sensor = Adafruit_Si7021();
  }

  void init()
  {
    sensor.begin();
    previousPerchMillis = 0;
    previousWeatherMillis = 0;
    currentLeftPerch = 1;
    currentRightPerch = 1;
    previousLeftPerch = 1;
    previousRightPerch = 1;
  }

  void Update()
  {
      // Serial.print("Feed Sensor: ");Serial.println(analogRead(A0));
      // Perch Activity updates/notifications depend on time and proximity sensors
      currentPerchMillis = millis();
      if ((currentPerchMillis - previousPerchMillis) > perchInterval )
      {
        previousPerchMillis = currentPerchMillis;
        currentLeftPerch = digitalRead(LEFT_PERCH);
        currentRightPerch = digitalRead(RIGHT_PERCH);
        if ((currentLeftPerch == 0) && (previousLeftPerch == 1))
        {
          // new activity on the leftperch
          // BLE notify
          perchCharacteristic.setValue(1);
          Serial.println("LEFT PERCH ACTIVITY");
          previousLeftPerch = 0;
        }
        else if ((currentLeftPerch == 1) && (previousLeftPerch == 0))
        {
          // flew off the perch, left perch now empty
          Serial.println("LEFT PERCH FLEW OFF");
          previousLeftPerch = 1;
        }
        if ((currentRightPerch == 0) && (previousRightPerch == 1))
        {
          // new activity on the  right perch
          // BLE notify
          Serial.println("RIGHT PERCH ACTIVITY");
          perchCharacteristic.setValue(2);
          previousRightPerch = 0;
        }
        else if ((currentRightPerch == 1) && (previousRightPerch == 0))
        {
          // flew off the perch, left perch now empty
          Serial.println("RIGHT PERCH FLEW OFF");
          previousRightPerch = 1;
        }
      }
      // temperature & humidity update, should convert to RTC based.
      currentWeatherMillis = millis();
      if ((currentWeatherMillis - previousWeatherMillis)>weatherInterval)
      {
        previousWeatherMillis = currentWeatherMillis;
        // BLE notify temp & humidity

        float tempT = sensor.readTemperature();
        float tempH = sensor.readHumidity();
        int tempF   = analogRead(A0);

        humidityData.h = tempH;
        tempData.t = tempT;
        feedData.f = tempF;

        Serial.print("Humidity:    "); Serial.print(humidityData.h, 2);
        Serial.print("\tTemperature: "); Serial.println(tempData.t, 2);

        Serial.print("Humidity:    "); Serial.print(tempH, 2);
        Serial.print("\tTemperature: "); Serial.println(tempT, 2);

        Serial.print("Feed: ");Serial.print(feedData.f);
        Serial.print("\tFeed: "); Serial.println(tempF);

        // tricky stuff here
        unsigned char *temp = (unsigned char *)&tempData;
        unsigned char *humidity = (unsigned char *)&humidityData;
        unsigned char *feed = (unsigned char *)&feedData;

        /**
          * Setting the values here will cause the BLE notification mechanism to be activated
          */
        tempCharacteristic.setValue(temp, 4);
        humidityCharacteristic.setValue(humidity, 4);
        feedCharacteristic.setValue(feed, 2);
      }
  }
};

BirdFeeder birdFeeder(1);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // only for debugging purposes, comment out for deployment

  // pinMode(BLEPIN, OUTPUT);

  // Setting up the BLE serice here
  blePeripheral.setLocalName("bird");
  blePeripheral.setAdvertisedServiceUuid(birdFeederService.uuid());  // add the service UUID
  blePeripheral.addAttribute(birdFeederService);
  blePeripheral.addAttribute(perchCharacteristic);
  blePeripheral.addAttribute(tempCharacteristic);
  blePeripheral.addAttribute(humidityCharacteristic);
  blePeripheral.addAttribute(feedCharacteristic);
  blePeripheral.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  blePeripheral.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // All characteristics should be initialized to a starting value prior
  // using them.
  const unsigned char initializeTemp[4] = {0,0,0,0};
  const unsigned char initializeHumidity[4] = {0,0,0,0};
  const unsigned char initializeFeed[2] = {0,0};

  tempCharacteristic.setValue(initializeTemp, 4);
  humidityCharacteristic.setValue(initializeHumidity, 4);
  feedCharacteristic.setValue(initializeFeed, 2);
  perchCharacteristic.setValue(0); // 0 = null, 1 = left perch occupied, 2 = right perch occupied

  birdFeeder.init();
  blePeripheral.begin();

}

void loop() {
  // put your main code here, to run repeatedly:
  blePeripheral.poll();
  birdFeeder.Update();
}

void blePeripheralConnectHandler(BLECentral& central)
{
  // central connected event handler
  // Serial.print("Connected event, central: ");
  // Serial.println(central.address());
  // digitalWrite(BLEPIN,HIGH);
}

void blePeripheralDisconnectHandler(BLECentral& central)
{
  // central disconnected event handler
  // Serial.print("Disconnected event, central: ");
  // Serial.println(central.address());
  // digitalWrite(BLEPIN,LOW);
}
