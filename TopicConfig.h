#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

const char *subscribe = "raihan/#";
const char *Sensor_Topic = "raihan/dataSensor";
const char *History = "raihan/dataHistory";
const char *setPoint = "raihan/dataSetPoint/Esp";
const char *setTime = "raihan/dataSetPoint/Esp/Timer";
const char *setTimeW = "raihan/dataSetPoint/Web/Timer";
const char *setPointW = "raihan/dataSetPoint/Web";
const char *setPointMW = "raihan/dataSetPointM/Web";
const char *ControllFan = "raihan/Controll/fan";
const char *ControllPump = "raihan/Controll/pump";
const char *ScheduleMode = "raihan/dataSetPoint/ScheduleMode";
const char *schedule1 = "raihan/dataSetSchedule1";
const char *schedule2 = "raihan/dataSetSchedule2";
const char *schedule3 = "raihan/dataSetSchedule3";
const char *setPointSW = "raihan/dataSetSchedule/Web";
