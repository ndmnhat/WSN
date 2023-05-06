#include "DHT.h"
#include "MPU6050_tockn.h"
#include "U8glib.h"
#include "Wire.h"

#include <SPI.h>
#include <LoRa.h>
#include <encryption.h>

const uint8_t RAIN_GAUGE_SENSOR = 4;
const uint8_t DHT_SENSOR = 3;

const int DELAY = 50;
const int MIN_RAIN_STATE_CHANGE = 100;
const int MAX_RAIN_STATE_CHANGE = 10000;
const int REPORT_DELAY = 10000;
const int MIN_VIBRATION_SAMPLE = 1000;
const int MAX_DELAY_DRAW = 2000;

const float RAIN_PER_HIT = 0.1;
const uint8_t DHTTYPE = DHT22;

struct {
    int state;
    unsigned long time;
    float velo;
} rainGauge;

struct {
    unsigned long time;
    float total;
    float value;
} vibration;

DHT dht(DHT_SENSOR, DHTTYPE);
MPU6050 mpu6050(Wire);
U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);

unsigned long lastMillis = 0;
unsigned long lastDraw = 0;
uint8_t drawStep = 0;

void setup()
{
    Serial.begin(9600);
	while (!Serial);

    pinMode(RAIN_GAUGE_SENSOR, INPUT_PULLUP);

    rainGauge.state = 0;
    rainGauge.time = 0;

    vibration.time = 0;
    vibration.total = 0;
    vibration.value = 0;

    dht.begin();

    Wire.begin();
    mpu6050.begin();

    if (u8g.getMode() == U8G_MODE_R3G3B2) {
        u8g.setColorIndex(255); // white
    } else if (u8g.getMode() == U8G_MODE_GRAY2BIT) {
        u8g.setColorIndex(3); // max intensity
    } else if (u8g.getMode() == U8G_MODE_BW) {
        u8g.setColorIndex(1); // pixel on
    } else if (u8g.getMode() == U8G_MODE_HICOLOR) {
        u8g.setHiColorByRGB(255, 255, 255);
    }

    if (!LoRa.begin(433E6)) {
		u8g.firstPage();
		do {
    		u8g.setFont(u8g_font_unifont);
        	u8g.drawStr(0, 22, "LoRa init failed.");
		} while (u8g.nextPage());

        while (true);
    }
	LoRa.setSyncWord(0xF3);
}

void draw(void)
{
    u8g.setFont(u8g_font_unifont);
	char formattedText[64];
	char float1[8];
    switch (drawStep) {
    case 0:
        u8g.drawStr(0, 22, "Hello, Master!");
        break;

    case 1:
        u8g.drawStr(0, 22, "Rain Velocity: ");
        break;

    case 2:
		dtostrf(rainGauge.velo, 0, 2, float1);
		snprintf(formattedText, 64, "%s hit/s", float1);
        u8g.drawStr(0, 22, formattedText);
        break;

    case 3:
		dtostrf(rainGauge.velo * RAIN_PER_HIT, 0, 2, float1);
		snprintf(formattedText, 64, " ~ %s mm/s", float1);
        u8g.drawStr(0, 22, formattedText);
        break;

    case 4:
        u8g.drawStr(0, 22, "Temperature: ");
        break;

    case 5:
		dtostrf(dht.readTemperature(), 0, 1, float1);
		snprintf(formattedText, 64, "%s °C", float1);
        u8g.drawStr(0, 22, formattedText);
        break;

    case 6:
        u8g.drawStr(0, 22, "Humidity: ");
        break;

    case 7:
		dtostrf(dht.readHumidity(), 0, 1, float1);
		snprintf(formattedText, 64, "%s %%", float1);
        u8g.drawStr(0, 22, formattedText);
        break;

    case 8:
        u8g.drawStr(0, 22, "Vibration: ");
        break;

    case 9:
		dtostrf(vibration.value, 0, 4, float1);
		snprintf(formattedText, 64, "%s ?/s", float1);
        u8g.drawStr(0, 22, formattedText);
        break;

    default:
        u8g.drawStr(0, 22, "Measuring...");
    }
}

void report()
{
	char formattedText[256];
    Serial.println("----- REPORT -----");
	
	char float1[8];
	char float2[8];
	char float3[8];
	char float4[8];
	char float5[8];

	dtostrf(rainGauge.velo, 0, 2, float1);
	dtostrf(rainGauge.velo * RAIN_PER_HIT, 0, 2, float2);
	dtostrf(dht.readTemperature(), 0, 1, float3);
	dtostrf(dht.readHumidity(), 0, 1, float4);
	dtostrf(vibration.value, 0, 4, float5);

	snprintf(formattedText, 256, "Rain Velocity: %s hit/s ~ %s mm/s\nTemperature: %s\nHumidity: %s\nVibration: %s\n", float1, float2, float3, float4, float5);
	Serial.println(formattedText);
}

void send_report()
{
	while (LoRa.beginPacket() == 0) {
		return;
  	}

	char formattedData[128];

	char float1[8];
	char float2[8];
	char float3[8];
	char float4[8];
	char float5[8];

	dtostrf(rainGauge.velo, 0, 2, float1);
	dtostrf(rainGauge.velo * RAIN_PER_HIT, 0, 2, float2);
	dtostrf(dht.readTemperature(), 0, 1, float3);
	dtostrf(dht.readHumidity(), 0, 1, float4);
	dtostrf(vibration.value, 0, 4, float5);

    uint8_t formattedDataLength = snprintf(formattedData, 128, "%s %s %s %s %s", float1, float2, float3, float4, float5);
    int base64Length = encrypted_base64_length(formattedDataLength);
	
    uint8_t base64Output[base64Length];
    encrypt_to_base64(base64Output, (uint8_t*)formattedData, formattedDataLength);
	
	LoRa.beginPacket();
  	LoRa.write((const uint8_t*)(base64Output), base64Length);
	LoRa.write(0x20);
	LoRa.write(get_iv(), 16);
  	LoRa.endPacket(true); // true = async / non-blocking mode

	free(base64Output);
}

void loop()
{
    unsigned long currentMillis = millis();

    int rainState = digitalRead(RAIN_GAUGE_SENSOR);
    if (rainState != rainGauge.state && currentMillis - rainGauge.time >= MIN_RAIN_STATE_CHANGE) {
        float rainVelocity = 1 / ((currentMillis - rainGauge.time) * 1.0 / 1000);

        rainGauge.state = rainState;
        rainGauge.time = currentMillis;
        rainGauge.velo = rainVelocity;
    }

    if (currentMillis - rainGauge.time >= MAX_RAIN_STATE_CHANGE) {
        rainGauge.time = currentMillis;
        rainGauge.velo = 0;
    }

    if (currentMillis - lastMillis >= REPORT_DELAY) {
        lastMillis = currentMillis;
		send_report();
        report();
    }

    mpu6050.update();
    float x = mpu6050.getAccX();
    float y = mpu6050.getAccY();
    float tot = x * y;
    vibration.total += abs(tot);

    if (currentMillis - vibration.time >= MIN_VIBRATION_SAMPLE) {
        vibration.value = vibration.total / (currentMillis - vibration.time) * 1000;
        vibration.total = 0;
        vibration.time = currentMillis;
    }

    if (currentMillis - lastDraw >= MAX_DELAY_DRAW) {
        u8g.firstPage();
        do {
            draw();
        } while (u8g.nextPage());

        drawStep = (drawStep + 1) % 10;
        lastDraw = currentMillis;
    }

    delay(DELAY);
}