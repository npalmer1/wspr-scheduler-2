// WiFi antenna switch sketch from Arduino.cs, with three fixes:
//  1. the pin number is read in full (the original read one digit, so "set-pin 12 high" set pin 1);
//  2. all switch pins 0-13 are cleared before the new one is set (the original cleared only 0-7);
//  3. the built-in LED is not driven when it is one of the switch pins (pin 13 on many boards).
// Protocol and replies are unchanged: "set-pin <n> high|low" -> "Pin: <n> set: high|low", else "error".
#include <WiFi.h>

const char *ssid = "wifi_ssid";          // replace with yours
const char *password = "wifi_password";  // ditto
const int kMaxPin = 13;                  // highest switch output pin

WiFiServer server(5000);

void setup()
{
  WiFi.setHostname("ArduinoSw");
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  if (LED_BUILTIN > kMaxPin)
    pinMode(LED_BUILTIN, OUTPUT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  server.begin();
  Serial.println("Server started");
  delay(1000);
  Serial.println(WiFi.localIP());
}

void loop()
{
  WiFiClient client = server.available();
  if (!client)
    return;
  Serial.println("Client connected");
  while (client.connected()) {
    if (!client.available())
      continue;
    String data = client.readStringUntil('\n');
    data.trim();
    Serial.println("Received: " + data);
    String msg = "error";
    const String prefix = "set-pin ";
    if (data.startsWith(prefix)) {
      String rest = data.substring(prefix.length());   // "<n> high" or "<n> low"
      int space = rest.indexOf(' ');
      String num = space > 0 ? rest.substring(0, space) : rest;
      int pin = num.toInt();
      bool valid = num.length() > 0 && pin >= 0 && pin <= kMaxPin && (num == String(pin));
      if (valid) {
        int hilo = rest.indexOf("high") > 0 ? HIGH : LOW;
        for (int i = 0; i <= kMaxPin; i++) {
          if (i != pin) {
            pinMode(i, OUTPUT);
            digitalWrite(i, LOW);
          }
        }
        // On many boards LED_BUILTIN is pin 13 - only use it as an indicator if it isn't a switch pin
        if (LED_BUILTIN > kMaxPin)
          digitalWrite(LED_BUILTIN, hilo);
        pinMode(pin, OUTPUT);
        digitalWrite(pin, hilo);
        msg = "Pin: " + String(pin) + " set: " + String(hilo == HIGH ? "high" : "low");
      }
    }
    delay(100);
    client.println(msg);
    Serial.println("Returned: " + msg);
    client.stop();
    Serial.println("Client disconnected");
  }
}
