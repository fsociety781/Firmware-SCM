#include "DHT.h"

#define DHTPIN 15
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

class dataSensor {
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
      float tempK = dht.readTemperature();
      float humK = dht.readHumidity();
      data.suhu =  tempK - 1.0;
      data.kelembaban = humK + 11.9;
      return data;
    };
};