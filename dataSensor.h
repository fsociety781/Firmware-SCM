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
    };

    static DHT_Values getValues(void)
    {
      DHT_Values data;
      data.suhu =  dht.readTemperature();
      data.kelembaban = dht.readHumidity();
      return data;
    };
};