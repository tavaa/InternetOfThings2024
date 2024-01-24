// Send data to thingspeak via MQTT di BME280 with deepsleep
// The server MQTT automatically disconnect after send, it's necessary to reconnect
// ESP8266-Essential connect pin16 with RST and add 220nF capacitor between RST and GND
// BME280  on pin 12(SCL) e 13(SDA)

// https://it.mathworks.com/help/thingspeak/use-arduino-client-to-publish-to-a-channel.html

#include "PubSubClient.h"
#include <ESP8266WiFi.h> 
#include "secrets.h"

bool DEBUG = true; // true=serial message of debug enabled

const char* server = "mqtt.thingspeak.com";
//mqtt3.thingspeak.com

WiFiClient wifiClient;
PubSubClient client(server, 1883, wifiClient);

String payload;

// BME280 Setting
#include <Wire.h>
#include <Adafruit_BME280.h>
//#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // I2C
bool BMEStatus;

ADC_MODE(ADC_VCC);  // Set ADC for read Vcc

// Update time in seconds. Min with Thingspeak is ~20 seconds
//const int UPDATE_INTERVAL_SECONDS = 3600;  //il clock interno ha un errore del 5% questo valore va tarato sperimentalmente - ogni ora
const int UPDATE_INTERVAL_SECONDS = 60;  // caricamento ogni minuto solo per test

void setup() 
{  
    // Connect BME280 GND TO pin14 OR board's GND
    pinMode(14, OUTPUT);
    digitalWrite(14, LOW); 
    
    Serial.begin(115200);
    delay(10);

    // BME280 Initialise I2C communication as MASTER
    Wire.begin(13, 12);  //Wire.begin([SDA], [SCL])
   
    BMEStatus = bme.begin();  
    if (!BMEStatus) 
    {
       if (DEBUG) { Serial.println("Could not find BME280!"); }
       //while (1);
    }

    // Weather monitoring See chapter 3.5 Recommended modes of operation
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
   

        // read values from the sensor
    float temperature, humidity, pressure;
    
    if (BMEStatus) 
    {
        temperature = bme.readTemperature();
        humidity = bme.readHumidity();
        pressure = bme.readPressure() / 100.0F;
    }  
    else
    {
        if (DEBUG) Serial.println("Could not find BME280!");
        temperature=0;
        humidity=0;
        pressure=0;
    }
 
    float voltage = ESP.getVcc();
    voltage = voltage/1024.0; //volt  

    if (DEBUG) 
    {
      Serial.println("T= " + String(temperature) + "°C  H= " + String(humidity) + "%  P=" + String(pressure) + "hPa  V=" + voltage + "V"); 
    }  

    // Construct MQTT payload
    payload="field1=";
    payload+=temperature;
    payload+="&field2=";
    payload+=humidity;
    payload+="&field3=";
    payload+=pressure;    
    payload+="&field4=";
    payload+=voltage;    
    payload+="&status=MQTTPUBLISH";
 
  
    //Connect to Wifi
    if (DEBUG) 
    {
      Serial.println();
      Serial.print("\nConnecting to WiFi SSID  ");
      Serial.print(SECRET_SSID);
    }  
    
    WiFi.begin(SECRET_SSID, SECRET_PASS);

    int timeOut=10; // Time out to connect is 10 seconds
    while ((WiFi.status() != WL_CONNECTED) && timeOut>0) 
    {
        delay(1000);
        if (DEBUG) { Serial.print("."); }
        timeOut--;
    }

    if (timeOut==0)  //No WiFi!
    {
        if (DEBUG) Serial.println("\nTimeOut Connection, go to sleep!\n\n");
        ESP.deepSleep(1E6 * UPDATE_INTERVAL_SECONDS);
    }

    if (DEBUG) // Yes WiFi
    {
      Serial.print("\nWiFi connected with IP address: ");  
      Serial.println(WiFi.localIP());
    }
    
    // Reconnect if MQTT client is not connected.
    if (!client.connected()) 
    {
      reconnect();
    }
  
    mqttpublish();   

    delay(200);  // Waiting for transmission to complete!!! (ci vuole)
    WiFi.disconnect( true );
    delay( 1 );
    
    if (DEBUG)  { Serial.println("Go to sleep!\n\n"); }
    // Sleep ESP and disable wifi at wakeup
    ESP.deepSleep( 1E6 * UPDATE_INTERVAL_SECONDS, WAKE_RF_DISABLED );  
}

void loop() 
{  
  //there's nothing to do
}

void mqttpublish()
{
    // read values from the sensor
    float temperature, humidity, pressure;
    
    if (DEBUG) 
    {
      Serial.print("Sending payload: ");
      Serial.println(payload);
    }  

    // Create a topic string and publish data to ThingSpeak channel feed. 
    String topicString ="channels/" + String( channelID ) + "/publish/"+String(writeAPIKey);
    unsigned int length=topicString.length();
    char topicBuffer[length];
    topicString.toCharArray(topicBuffer,length+1);
    
    if (client.publish(topicBuffer, (char*) payload.c_str())) 
    {
            if (DEBUG) Serial.println("Publish ok");
    }
    else
    {
            if (DEBUG) Serial.println("Publish failed");
    }  
}

void reconnect() 
{
    String clientName="MY-ESP";
    // Loop until we're reconnected
    while (!client.connected()) 
    {
        if (DEBUG) Serial.println("Attempting MQTT connection...");
        // Try to connect to the MQTT broker
        if (client.connect((char*) clientName.c_str())) 
        {
            if (DEBUG) Serial.println("Connected");
        } 
        else 
        {
            if (DEBUG) 
            {
              Serial.print("failed, try again");
              // Print to know why the connection failed.
              // See http://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
              Serial.print(client.state());
              Serial.println(" try again in 2 seconds");
            }  
            delay(2000);  
        }
    }
}
