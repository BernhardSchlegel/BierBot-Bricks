#define WM_NODEBUG
#define ARDUINO_ESP8266_RELEASE_2_3_0

#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <OneWire.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "helper.h"

// Set web server port number to 80
WiFiServer server(80);

void handleRoot(); // function prototypes for HTTP handlers
void handleNotFound();

OneWire ds(14); // on pin 14 SONOFF TH16

#define EEPROM_ADDRESS_APIKEY 0
#define GPIO_TRIGGER 0
#define GPIO_RELAIS 12
#define GPIO_LED 13
#define HTTP_REQUEST_RESPONSE_BUF_LEN 255

String apikey;
WiFiManager wm;                           // global wm instance
WiFiManagerParameter custom_field_apikey; // global param ( for non blocking w params )
HTTPClient http;                          //Declare an object of class HTTPClient
WiFiClientSecure https;

uint8_t global_error = 0;
uint8_t global_warning = 0;
String global_error_text = "";
String global_warning_text = "";
float celsius, fahrenheit;
String chipid = "";
uint32_t main_interval_ms = 1000; // 1s default intervall for first iteration
uint8_t global_relais_state = 0;
String global_version = "0.9.0";

void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  Serial.print("String has length ");
  Serial.println(len);
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    // replace values in byte-array cache with modified data
    // no changes made to flash, all in local byte-array cache
    EEPROM.put(addrOffset + 1 + i, strToWrite[i]);
  }
  Serial.println("Data put to EEPROM.");

  // https://arduino.stackexchange.com/questions/25945/how-to-read-and-write-eeprom-in-esp8266
  // actually write the content of byte-array cache to
  // hardware flash.  flash write occurs if and only if one or more byte
  // in byte-array cache has been changed, but if so, ALL 512 bytes are
  // written to flash
  EEPROM.commit();
  Serial.println("EEPROM committed.");
}

String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0'; // the character may appear in a weird way, you should read: 'only one backslash and 0'
  return String(data);
}

String getParam(String name)
{
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback()
{
  Serial.println("[CALLBACK] saveParamCallback fired");
  apikey = getParam("apikey").substring(0, 20);
  Serial.println("PARAM apikey straight = " + apikey);
  //String apikey = getParam("apikey");
  writeStringToEEPROM(EEPROM_ADDRESS_APIKEY, apikey);
  String apikey_restored = readStringFromEEPROM(EEPROM_ADDRESS_APIKEY);
  Serial.println("PARAM apikey from EEPROM = " + apikey_restored);
}

void checkButton()
{
  // check for button press
  if (digitalRead(GPIO_TRIGGER) == LOW)
  {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(GPIO_TRIGGER) == LOW)
    {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if (digitalRead(GPIO_TRIGGER) == LOW)
      {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.disconnect();
        wm.resetSettings();
        ESP.restart();
      }
      else
      {
        server.stop(); // kill status server (to avoid conflicnt)

        // start portal w delay
        Serial.println("Starting config portal");
        wm.setConfigPortalTimeout(120);

        if (!wm.startConfigPortal("BierBot Brick 101"))
        {
          Serial.println("failed to connect or hit timeout");
          delay(3000);
          // ESP.restart();
        }
        else
        {
          //if you get here you have connected to the WiFi
          Serial.println("connection re-established");
        }

        server.begin(); // start status server again
      }
    }
  }
}

void short_flash_500ms(uint8_t count)
{
  for (uint8_t i = 0; i < count; ++i)
  {
    digitalWrite(GPIO_LED, 1);
    delay(100);
    digitalWrite(GPIO_LED, 0);
    delay(400);
  }
}

void setup()
{
  Serial.println("BierBot Brick 101 starting");
  chipid = String(ESP.getFlashChipId()) + "_" + String(WiFi.macAddress());
  Serial.println("chipid: " + chipid);
  short_flash_500ms(10);

  //---------------------------------------------------------------------------------------
  // GPIO
  //---------------------------------------------------------------------------------------
  pinMode(GPIO_TRIGGER, INPUT);
  pinMode(GPIO_RELAIS, OUTPUT);
  pinMode(GPIO_LED, OUTPUT);

  //---------------------------------------------------------------------------------------
  // EEPROM
  //---------------------------------------------------------------------------------------
  // commit 512 bytes of ESP8266 flash (for "EEPROM" emulation)
  // this step actually loads the content (512 bytes) of flash into
  // a 512-byte-array cache in RAM
  EEPROM.begin(512);

  //---------------------------------------------------------------------------------------
  // WIFI
  //---------------------------------------------------------------------------------------
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("\n Starting");

  // guid, len 36 + terminator
  // e.g. 550e8400-e29b-11d4-a716-446655440000
  // test custom html(radio)
  //const char* custom_apikey_str = "<br/><label for='apikey'>API key</label><br><input type='text' name='apikey' value='1' checked>";
  //new (&custom_field_apikey) WiFiManagerParameter(custom_apikey_str); // custom html input
  new (&custom_field_apikey) WiFiManagerParameter("apikey", "BierBot Bricks API Key", "aZ42ILoveBrewingB33r", 37, "placeholder=\"get your API key at bricks.bierbot.com\"");

  wm.addParameter(&custom_field_apikey);
  //wm.setConfigPortalBlocking(false);
  wm.setSaveConfigCallback(saveParamCallback);

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("BierBot Brick 101"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect");
    // ESP.restart();
  }
  else
  {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }

  // eeprom for storing api key
  EEPROM.begin(512);

  server.begin();
}

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

void addDataRow(WiFiClient client, String key, String value)
{
  client.println("<tr>");
  client.println("<td>");
  client.println(key);
  client.println("</td>");
  client.println("<td>");
  client.println(value);
  client.println("</td>");
  client.println("</tr>");
}

void checkRequest()
{

  WiFiClient client = server.available(); // Listen for incoming clients

  if (client)
  {                                // If a new client connects,
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
      currentTime = millis();
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");

            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>BierBot Brick 101</h1>");
            client.println("<table>");
            String temp = String(celsius);
            addDataRow(client, "Temperature", temp);
            String api_key = readStringFromEEPROM(EEPROM_ADDRESS_APIKEY);
            addDataRow(client, "API key", api_key);

            if (global_warning)
            {
              addDataRow(client, "Warning", global_warning_text);
            }
            if (global_error)
            {
              addDataRow(client, "Warning", global_error_text);
            }

            client.println("</body></html>");
            client.println("<table>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void readTemperature()
{
  // put your main code here, to run repeatedly:
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];

  if (!ds.search(addr))
  {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }

  Serial.print("ROM =");
  for (i = 0; i < 8; i++)
  {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7])
  {
    Serial.println("CRC is not valid!");
    return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0])
  {
  case 0x10:
    Serial.println("  Chip = DS18S20"); // or old DS1820
    type_s = 1;
    break;
  case 0x28:
    Serial.println("  Chip = DS18B20");
    type_s = 0;
    break;
  case 0x22:
    Serial.println("  Chip = DS1822");
    type_s = 0;
    break;
  default:
    Serial.println("Device is not a DS18x20 family device.");
    return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for (i = 0; i < 9; i++)
  { // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
                      //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.println(" Celsius.");
  //Serial.print(fahrenheit);
  //Serial.println(" Fahrenheit");

  String apikey_restored = readStringFromEEPROM(EEPROM_ADDRESS_APIKEY);
  Serial.println("PARAM apikey from EEPROM = " + apikey_restored);
}

void contactBackend()
{
  if (1 == 1)
  { // WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    String apikey_restored = readStringFromEEPROM(EEPROM_ADDRESS_APIKEY);
    String temp = String(celsius);
    String relais = String(global_relais_state);
    String url = "https://cloud.bierbot.com/api/iot/v1?apikey=" + apikey_restored + "&type=" + "sonoff_th16" + "&brand=" + "bierbot" + "&version=" + global_version + "&s_number_temp_0=" + temp + "&a_bool_epower_0=" + relais + "&chipid=" + chipid; // + "&temp=" + temp + "&type=" + "sonoff_th16" + "&version=" + global_version

    Serial.print("s_number_temp_0=" + temp);
    Serial.println(", a_bool_epower_0=" + relais);

    WiFiClientSecure client;
    client.setInsecure(); // unfortunately necessary, ESP8266 does not support SSL without hard coding certificates
    client.connect(url, 443);

    http.begin(client, url);

    // String payload = "{\"s_number_temp_0\":\"" + temp + "\",\"a_bool_epower_0\":\"" + relais + "\",\"type\":\"" + "sonoff_th16" + "\",\"brand\":\"" + "bierbot" + "\",\"version\":\"" + global_version + "\",\"chipid\":\"" + chipid + "\"}";
    // Serial.println("payload " + payload);

    Serial.println("submitting POST to " + url);
    int httpCode = http.GET(); // GET has issues with 301 forwards
    Serial.print("httpCode: ");
    Serial.println(httpCode);

    if (httpCode > 0)
    { //Check the returning code

      for (int i = 0; i < http.headers(); i++)
      {
        Serial.println(http.header(i));
      }

      String payload = http.getString(); //Get the request response payload
      Serial.print(F("received payload: "));
      Serial.println(payload); //Print the response payload

      if (payload.length() == 0)
      {
        Serial.println(F("payload empty, skipping."));
        Serial.println(F("setting next request to 60s."));
        main_interval_ms = 60000;
      }
      else if (payload.indexOf("{") == -1)
      {
        Serial.println("no JSON - skippping.");
        Serial.println(F("setting next request to 60s."));
        main_interval_ms = 60000;
      }
      else
      {
        StaticJsonDocument<HTTP_REQUEST_RESPONSE_BUF_LEN> doc;

        // Deserialize the JSON document
        // doc should look like "{\"target_state\":0, \"next_request_ms\":10000,\"error\":1,\"error_text\":\"Your device will need an upgrade\",\"warning\":1,\"warning_text\":\"Your device will need an upgrade\"}";
        DeserializationError error = deserializeJson(doc, payload);
        // Test if parsing succeeds.
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          global_error = 1;
          global_error_text = String("JSON error: ") + String(error.f_str());
        }
        else
        {
          Serial.println("deserializeJson success");
          if (doc["error"])
          {
            // an error is critical, do not proceed with logik
            const char *error_text = doc["error_text"];
            global_error_text = String(error_text);
            global_error = 1;
          }
          else
          {
            global_error = 0;
            if (doc["warning"])
            {
              // proceed with logik, if there is a warning
              const char *warning_text = doc["warning_text"];
              global_warning_text = String(warning_text);
              global_warning = 1;
            }
            else
            {
              global_warning = 0;
            }

            // main logic here
            if (doc.containsKey("next_request_ms"))
            {
              main_interval_ms = doc["next_request_ms"].as<long>();
            }
            else
            {
              // try again in 30s
              main_interval_ms = 30000;
            }
            if (doc.containsKey("epower_0_state"))
            {
              global_relais_state = doc["epower_0_state"];
            }
          }
        }
      }
    }
    else
    {
      Serial.print(F("setting next request to 60s."));
      main_interval_ms = 60000;
    }
    http.end(); //Close connection
  }
};

void setRelais()
{
  digitalWrite(GPIO_RELAIS, global_relais_state);
  digitalWrite(GPIO_LED, global_relais_state);
};

void loop()
{
  checkRequest();
  checkButton();

  static uint32_t state_main_interval = 0;
  if (TimeReached(state_main_interval))
  {
    // one interval delay in case server wants to set new interval
    SetNextTimeInterval(state_main_interval, main_interval_ms);

    Serial.println("##### MAIN: reading temperature");
    readTemperature();

    Serial.println("##### MAIN: contacting backend");
    contactBackend();

    Serial.println("##### MAIN: setting relais");
    setRelais();
  }
}
