#include "temp_humi_monitor.h"

DHT20 dht20;
LiquidCrystal_I2C lcd(33, 16, 2);

// Nguong bien thien toi da cho 5 mau
static constexpr float TEMP_VARIATION_THRESHOLD = 1.0f; // do C
static constexpr float HUMI_VARIATION_THRESHOLD = 3.0f; // %RH
static constexpr int SAMPLE_COUNT = 5;
static constexpr TickType_t SAMPLE_INTERVAL = pdMS_TO_TICKS(100);

// He so EMA: gia tri moi chiem 30%, ket qua truoc chiem 70%
static constexpr float TEMP_EMA_ALPHA = 0.3f;
static constexpr float HUMI_EMA_ALPHA = 0.3f;

void temp_humi_monitor(void *pvParameters)
{
    // Khoi tao I2C
    Wire.begin(11, 12);

    // Khoi tao LCD
    lcd.begin();
    lcd.backlight();
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("YOLO UNO MONITOR");

    lcd.setCursor(0, 1);
    lcd.print("Initializing...");

    // Khoi tao DHT20
    dht20.begin();

    Serial.println("[DHT] LCD initialized!");

    // Trang thai bo loc EMA duoc giu lai qua cac chu ky doc cam bien
    float emaTemperature = 0.0f;
    float emaHumidity = 0.0f;
    bool emaInitialized = false;

    while (1)
    {
        float temperatureSamples[SAMPLE_COUNT];
        float humiditySamples[SAMPLE_COUNT];
        bool samplesValid = true;

        // Doc 5 mau, cach nhau 100 ms
        for (int i = 0; i < SAMPLE_COUNT; i++)
        {
            dht20.read();
            temperatureSamples[i] = dht20.getTemperature();
            humiditySamples[i] = dht20.getHumidity();

            // ===== MO PHONG LOI CAM BIEN =====
            /*
            bool simulateSensorError = true;
            if (simulateSensorError)
            {
                temperatureSamples[i] = 0.0f;
                humiditySamples[i] = 0.0f;
            }
            */

            if (isnan(temperatureSamples[i]) || isnan(humiditySamples[i]) ||
                (temperatureSamples[i] == 0.0f && humiditySamples[i] == 0.0f))
            {
                samplesValid = false;
            }

            // Chi can cho giua cac lan doc, khong can cho sau mau thu 5
            if (i < SAMPLE_COUNT - 1)
            {
                vTaskDelay(SAMPLE_INTERVAL);
            }
        }

        if (!samplesValid)
        {
            portENTER_CRITICAL(&temperatureMux);
            glob_sensor_error = true;
            portEXIT_CRITICAL(&temperatureMux);

            Serial.println("[DHT] Sensor Error: invalid sample!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Sensor Error");
            lcd.setCursor(0, 1);
            lcd.print("Check DHT20");
        }
        else
        {
            float temperatureSum = 0.0f;
            float humiditySum = 0.0f;
            float minTemperature = temperatureSamples[0];
            float maxTemperature = temperatureSamples[0];
            float minHumidity = humiditySamples[0];
            float maxHumidity = humiditySamples[0];

            for (int i = 0; i < SAMPLE_COUNT; i++)
            {
                temperatureSum += temperatureSamples[i];
                humiditySum += humiditySamples[i];

                if (temperatureSamples[i] < minTemperature)
                    minTemperature = temperatureSamples[i];
                if (temperatureSamples[i] > maxTemperature)
                    maxTemperature = temperatureSamples[i];
                if (humiditySamples[i] < minHumidity)
                    minHumidity = humiditySamples[i];
                if (humiditySamples[i] > maxHumidity)
                    maxHumidity = humiditySamples[i];
            }

            float temperatureVariation = maxTemperature - minTemperature;
            float humidityVariation = maxHumidity - minHumidity;

            // Chi chap nhan bo mau neu ca nhiet do va do am deu on dinh
            if (temperatureVariation < TEMP_VARIATION_THRESHOLD &&
                humidityVariation < HUMI_VARIATION_THRESHOLD)
            {
                float averageTemperature = temperatureSum / SAMPLE_COUNT;
                float averageHumidity = humiditySum / SAMPLE_COUNT;

                // Ap dung EMA len gia tri trung binh 5 mau cua bai 4.
                // Lan dau tien khoi tao EMA bang gia tri dau vao de tranh
                // lam ket qua bi keo ve 0.0f.
                if (!emaInitialized)
                {
                    emaTemperature = averageTemperature;
                    emaHumidity = averageHumidity;
                    emaInitialized = true;
                }
                else
                {
                    emaTemperature = TEMP_EMA_ALPHA * averageTemperature +
                                     (1.0f - TEMP_EMA_ALPHA) * emaTemperature;
                    emaHumidity = HUMI_EMA_ALPHA * averageHumidity +
                                  (1.0f - HUMI_EMA_ALPHA) * emaHumidity;
                }

                // Cap nhat bien toan cuc bang ket qua da qua loc EMA
                // va trang thai loi trong cung vung toi han.
                portENTER_CRITICAL(&temperatureMux);
                glob_temperature = emaTemperature;
                glob_temperature_valid = true;
                glob_sensor_error = false;
                portEXIT_CRITICAL(&temperatureMux);

                // Cap nhat do am toan cuc
                portENTER_CRITICAL(&humidityMux);
                glob_humidity = emaHumidity;
                portEXIT_CRITICAL(&humidityMux);

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Humi: ");
                lcd.print(emaHumidity, 1);
                lcd.print("%");

                lcd.setCursor(0, 1);
                lcd.print("Temp: ");
                lcd.print(emaTemperature, 1);
                lcd.print("C");

                Serial.printf("[DHT] 5-sample average: Humi = %.2f%%, Temp = %.2f C | EMA: Humi = %.2f%%, Temp = %.2f C\n",
                              averageHumidity, averageTemperature, emaHumidity, emaTemperature);
            }
            else
            {
                // Bo mau khong on dinh: giu nguyen cac bien toan cuc truoc do
                Serial.printf("[DHT] Unstable samples - Temp variation: %.2f C, Humi variation: %.2f%%\n",
                              temperatureVariation, humidityVariation);

                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Unstable data");
                lcd.setCursor(0, 1);
                lcd.print("Keep old value");
            }
        }

        // Chu ky giua hai bo 5 mau
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
