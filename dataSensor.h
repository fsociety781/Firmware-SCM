#include "DHT.h"

#define DHTPIN 15
#define DHTTYPE DHT22

const float slope_temp = 0.49048625792811795;
const float intercept_temp = 12.748202959830875;


DHT dht(DHTPIN, DHTTYPE);

class dataDHT {
  public:
      struct DHT_Values
      {
        float suhu;
        float kelembaban;
      };

    static void setup(void)
    {
      dht.begin();
    }
    static DHT_Values getValues(void)
    {
      DHT_Values data;
      float s = dht.readTemperature();
      float h = dht.readHumidity();
      // float calibrated_humidity = h * slope_hum + intercept_hum;
      float calibrated_temperature = s * slope_temp + intercept_temp;
      data.suhu =  calibrated_temperature - 0.2;
      data.kelembaban = h + 3.6;
      return data;
    };
};