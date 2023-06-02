#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h> // Include the Wi-Fi library
#include <ESP8266mDNS.h>
#include <LoRa.h>
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <decryption.h>
#include <secret.h>
#include <rfc3339.h>

String idToken = "";
String error = "";
bool authorized = false;

X509List cert(root_ca);

WiFiClientSecure client;
HTTPClient httpClient;

void printUint(uint8_t* data, int len)
{
    for (int i = 0; i < len; i++) {
        Serial.print((char)data[i]);
    }
    Serial.println();
}

String postJson(char* data, uint16_t contentLength, String url, String authorization = "")
{
    client.setTrustAnchors(&cert);
    httpClient.begin(client, url);
    httpClient.addHeader("Content-Type", "application/json");
    if (authorization != "")
    {
        httpClient.addHeader("Authorization", "Bearer " + authorization);
    }
    httpClient.addHeader("Content-Length", String(contentLength));

    httpClient.POST(data);
    
    Serial.println(data);

    String content = httpClient.getString();

    Serial.println(content);

    httpClient.end();

    return content;
}

void parseToken(String content)
{
    const size_t bufferSize = JSON_OBJECT_SIZE(7) + 2202;
    DynamicJsonDocument doc(bufferSize);
    deserializeJson(doc, content);

    if (doc.containsKey("error")) {
        error = doc["error"]["message"].as<String>();
        Serial.println("Error: " + error);
        authorized = false;
    } else {
        idToken = doc["access_token"].as<String>();
        authorized = true;
    }
}

void parseSensorsReading(String content)
{
    const size_t bufferSize = 5 * JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 300;
    DynamicJsonDocument doc(bufferSize);
    deserializeJson(doc, content);

    if (doc.containsKey("error")) {
        if (doc["error"]["code"].as<int>() == 401)
            authorized = false;
    }
}

void getToken()
{
    const size_t bufferSize = JSON_OBJECT_SIZE(2) + 266;
    DynamicJsonDocument doc(bufferSize);
    char data[bufferSize];

    JsonObject object = doc.to<JsonObject>();
    object["grant_type"] = "refresh_token";
    object["refresh_token"] = refreshToken;

    serializeJson(doc, data);
    uint16_t docLength = measureJson(doc);

    String response = postJson(data, docLength, getTokenURL);
    parseToken(response);
}

void postSensorsReading(uint8_t* rainVelo, uint8_t* rainVelo2, uint8_t* temp, uint8_t* humid, uint8_t* vibration)
{
    DynamicJsonDocument doc(1024);
    char data[1024];

    char nowStr[30];
    struct date_time_struct now;
    struct date_time_struct *nowPtr = &now;
    _utcnow(nowPtr);
    _format_date_time(nowPtr, nowStr);

    JsonObject object = doc.createNestedObject("fields");
    object.createNestedObject("rainVelo")["doubleValue"] = atof((char*)rainVelo);
    object.createNestedObject("rainVelo2")["doubleValue"] = atof((char*)rainVelo2);
    object.createNestedObject("temp")["doubleValue"] = atof((char*)temp);
    object.createNestedObject("humid")["doubleValue"] = atof((char*)humid);
    object.createNestedObject("vibration")["doubleValue"] = atof((char*)vibration);
    object.createNestedObject("timestamp")["timestampValue"] = nowStr;

    serializeJson(doc, data);
    uint16_t docLength = measureJson(doc);

    Serial.println(data);
    
    String response = postJson(data, docLength, sensorsReadingURL, idToken);

    parseSensorsReading(response);
}

void readSensorData(uint8_t* sensorData, int len)
{
    int rainvelo = 0;
    int rainvelo2 = 0;
    int temp=0;
    int humid=0;
    int vibration=len;
    int counter = 0;
    
    for(int i = 0; i < len ; ++i)
    {
        if(sensorData[i] == 0x20)
        {
            switch (++counter)
            {
                case 1:
                    rainvelo = i;
                    break;
                case 2:
                    rainvelo2 = i;
                    break;
                case 3:
                    temp = i;
                    break;
                case 4:
                    humid = i;
                    break;
                default:
                    break;
            }
        }
    }

    uint8_t rainveloData[rainvelo];
    uint8_t rainvelo2Data[rainvelo2 - rainvelo - 1];
    uint8_t tempData[temp - rainvelo2 - 1];
    uint8_t humidData[humid - temp - 1];
    uint8_t vibrationData[vibration - humid - 1];
    
    memcpy(rainveloData, sensorData, rainvelo);
    memcpy(rainvelo2Data, sensorData + rainvelo + 1, rainvelo2 - rainvelo - 1);
    memcpy(tempData, sensorData + rainvelo2 + 1, temp - rainvelo2 - 1);
    memcpy(humidData, sensorData + temp + 1, humid - temp - 1);
    memcpy(vibrationData, sensorData + humid + 1, vibration - humid - 1);

    printUint(rainveloData, rainvelo);
    printUint(rainvelo2Data, rainvelo2 - rainvelo - 1);
    printUint(tempData, temp - rainvelo2 - 1);
    printUint(humidData, humid - temp - 1);
    printUint(vibrationData, vibration - humid - 1);

    postSensorsReading(rainveloData, rainvelo2Data, tempData, humidData, vibrationData);
}

void LoRa_rxMode()
{
    // LoRa.enableInvertIQ(); // active invert I and Q signals
    LoRa.receive(); // set receive mode
}

int base64_length(uint8_t* packet, int len)
{
    int base64len = 0;
    for(int i = 0; i < len ; ++i)
    {
        if(packet[i] == 0x20)
        {
            base64len = i;
        }
    }

    return base64len;
}
int iv_length(int base64len, int packetlen)
{
    return packetlen - base64len;
}

void initWiFi()
{
    WiFi.begin(ssid, password); // Connect to the network
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println(" ...");

    if (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
        delay(1000);
    }

    // Set time via NTP, as required for x.509 validation
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    Serial.print("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 1676426912) { // a random timestamp from the past
        delay(500);
        Serial.print(".");
        now = time(nullptr);
    }

    Serial.println('\n');
    Serial.println("Connection established!");
}

void initLoRa()
{
    LoRa.setPins(D8, D0, D2);
    while (!LoRa.begin(433E6)) 
    {
        Serial.println("Starting LoRa failed!");
        delay(100);
        while (1);
    }
    LoRa.setSyncWord(0xF3);
    // LoRa.onReceive(onReceive);
    // LoRa_rxMode();
    Serial.println("LoRa Initializing OK!");
}

void setup()
{
    Serial.begin(115200); // Start the Serial communication to send messages to the computer

    while (!Serial)
        ;

    Serial.println('\n');

    initLoRa();
    Serial.println("Wi-Fi");
    initWiFi();
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
        delay(1000);
        Serial.println("Connecting to WiFi..");
        return;
    }

    if (idToken == NULL || idToken == "null" || !authorized) {
        Serial.println("Geting token...");
        getToken();
        delay(2000);
        return;
    }
    
    int packetSize = LoRa.parsePacket();
    if(packetSize) {
        // received a packet
        Serial.print("Received packet: ");

        // read packet
        while (LoRa.available()) {
            uint8_t sensorData[packetSize];
            LoRa.readBytes(sensorData, packetSize);
            printUint(sensorData, packetSize);

            int base64len = base64_length(sensorData, packetSize);
            int ivlen = iv_length(base64len, packetSize);

            uint8_t iv[ivlen];
            uint8_t base64[base64len];

            memcpy(base64, sensorData, base64len);
            memcpy(iv, sensorData + (base64len +1), ivlen);

            int decryptedlen = decrypted_len(base64, base64len);
            uint8_t decrypted[decryptedlen];

            decrypt_to_base64(decrypted, base64, base64len, iv);

            readSensorData(decrypted, decryptedlen);
        }
    }
    delay(50);
    
}
