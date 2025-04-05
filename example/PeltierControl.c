/**
 * @file PeltierControl.c
 *
 * To-Do:  
 * 1 . testing mqtt broker
 */

#include "fuzzyc.h"
#include "MQTTClient.h"
#include "softPwm.h"
#include "unistd.h"
#include "wiringPi.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sensor and PWM
#define PWM_RANGE   70
#define COOLER_PIN  23
#define HEATER_PIN  24
#define SENSOR_PATH "/sys/bus/w1/devices/28-3ce1d4434496/w1_slave"

// MQTT Broker
#define MQTT_ADDRESS     "mqtt.flespi.io"
#define MQTT_PORT        "1883"
#define MQTT_CLIENTID    "WaterTank"
#define MQTT_USERNAME    "zVVYZfoyMExlpmsIW4kEe2RKlF4ZVYpynLvlCLJBcFhcrWFKs6aMCym2GQgBIODh"  
#define MQTT_PASSWORD    ""    // No need
#define MQTT_QOS         1
#define TIMEOUT     10000L
#define MQTT_TOKEN  ""

#define TOPIC_TEMP              "watertank/temp"
#define TOPIC_TEMP_CHANGE       "watertank/temp_change"
#define TOPIC_PELTIER_COOL      "watertank/cooler"
#define TOPIC_PELTIER_HEAT      "watertank/heater"
// Define the labels for the fuzzy sets (only used for debugging)
const char *tempLabels[] = {"Cool", "Normal", "Hot"}; // Lanh <> Binh thuong <> Nong
const char *changeLabels[] = {"Dec", "Stable", "Inc"}; // Giam <> On dinh <> Tang
const char *peltierSpeedLabels[] = {"Off", "Slow", "Medium", "Fast"};

// Define the input fuzzy sets
FuzzySet_t TemperatureState; // Trang thai nhiet do
FuzzySet_t TempChangeState;  // Trang thai thay doi nhiet do

// Define the output fuzzy set
FuzzySet_t PelCoolerSpeed; // Toc do cua may lam mat
FuzzySet_t PelHeaterSpeed; // Toc do cua may lam nong


 MQTTClient client;
int is_connected = 0;
// Read sensor temperature
double get_Temperature(const char *sensor_path) {

    FILE *fp;
    char buf[256];
    char *temp_str;
    double temperature = -1.0;

    fp = fopen(sensor_path, "r");
    if (fp == NULL) {
        perror("Failed to open sensor file");
        return -1;
    }

    // Read the file contents
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        // Find "t=" in the string
        if ((temp_str = strstr(buf, "t=")) != NULL) {
            // Convert to float (temp in millidegree Celsius)
            temperature = atof(temp_str + 2) / 1000.0;
            break;
        }
    }

    fclose(fp);
    return temperature;
}

void setPeltierCoolPower(int coolerPower) {
    softPwmWrite(COOLER_PIN, coolerPower);
}

void setPeltierHeatPower(int heaterPower) {
    softPwmWrite(HEATER_PIN, heaterPower);
}
// Define the membership functions for the fuzzy sets
/*
   >> NEED TO FIND CORRECT VALUES FOR MEMBERSHIP FUNCTIONS <<
   // TRAPEZOIDAL: Hinh Thang
   // TRIANGULAR : Tam Giac
   // RECTANGULAR: Chu Nhat
*/
#define TemperatureMembershipFunctions(X)                                      \
    X(TEMPERATURE_LOW, -10.0, 0.0, 20.0, 28.0, TRAPEZOIDAL)                    \
    X(TEMPERATURE_MEDIUM, 26.0, 30.0, 38.0, TRIANGULAR)                        \
    X(TEMPERATURE_HIGH, 28.0, 38.0, 100.0, 100.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TemperatureMembershipFunctions)

#define TempChangeMembershipFunctions(X)                                       \
    X(TEMP_CHANGE_DECREASING, -50.0, -20.0, -1.0, 0.0, TRAPEZOIDAL)            \
    X(TEMP_CHANGE_STABLE, -1.0, 0.0, 1.0, TRIANGULAR)                          \
    X(TEMP_CHANGE_INCREASING, 0.0, 1.0, 20.0, 50.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TempChangeMembershipFunctions)
//
#define PeltierCoolerSpeedMembershipFunctions(X)                               \
    X(PELTIER_COOLER_SPEED_OFF, -20.0, 0.0, 0.0, 0.0, TRAPEZOIDAL)             \
    X(PELTIER_COOLER_SPEED_SLOW, 00.0, 10.0, 15.0, 25.0, TRAPEZOIDAL)          \
    X(PELTIER_COOLER_SPEED_MEDIUM, 15.0, 25.0, 30.0, 40.0, TRAPEZOIDAL)        \
    X(PELTIER_COOLER_SPEED_FAST, 35.0, 45.0, 70.0, 70.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(PeltierCoolerSpeedMembershipFunctions)

#define PeltierHeaterSpeedMembershipFunctions(X)                               \
    X(PELTIER_HEATER_SPEED_OFF, -20.0, 0.0, 0.0, 0.0, TRAPEZOIDAL)             \
    X(PELTIER_HEATER_SPEED_SLOW, 00.0, 10.0, 15.0, 25.0, TRAPEZOIDAL)          \
    X(PELTIER_HEATER_SPEED_MEDIUM, 15.0, 25.0, 30.0, 40.0, TRAPEZOIDAL)        \
    X(PELTIER_HEATER_SPEED_FAST, 35.0, 45.0, 70.0, 70.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(PeltierHeaterSpeedMembershipFunctions)
// Define the fuzzy rules
/*
    >> NEED TO DEFINE THE RULES FOR THE SYSTEM <<
*/
FuzzyRule_t rules[] = {

    // Rule 1: Temp low and decreasing => Slow
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                            VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_FAST)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                           VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_OFF)),

    // Rule 2:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_MEDIUM)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_OFF)),

    // Rule 3:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_SLOW)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                           VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_OFF)),
    // Rule 4:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_SLOW)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_OFF)),
    // Rule 5:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                           VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_SLOW)),
    // Rule 6:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                           VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_OFF)),
    // Rule 7:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                            VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),

    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_SLOW)),
    // Rule 8: 
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_MEDIUM)),
    // Rule 9: 
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_FAST)),
};

// Init the fuzzy classifiers
void createClassifiers() {
    FuzzySetInit(&TemperatureState, TemperatureMembershipFunctions,
                 FUZZY_LENGTH(TemperatureMembershipFunctions));
    FuzzySetInit(&TempChangeState, TempChangeMembershipFunctions,
                 FUZZY_LENGTH(TempChangeMembershipFunctions));

    FuzzySetInit(&PelCoolerSpeed, PeltierCoolerSpeedMembershipFunctions,
                 FUZZY_LENGTH(PeltierCoolerSpeedMembershipFunctions));
    FuzzySetInit(&PelHeaterSpeed, PeltierHeaterSpeedMembershipFunctions,
                 FUZZY_LENGTH(PeltierHeaterSpeedMembershipFunctions));
}

// Helper function to destroy the fuzzy classifiers
void destroyClassifiers() {
    FuzzySetFree(&TemperatureState);
    FuzzySetFree(&TempChangeState);

    FuzzySetFree(&PelCoolerSpeed);
    FuzzySetFree(&PelHeaterSpeed);
}

int mqtt_connect() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // Tạo client MQTT
    MQTTClient_create(&client, MQTT_ADDRESS, MQTT_CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = MQTT_USERNAME;
    conn_opts.password = MQTT_PASSWORD;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", rc);
        return 0;
    }
    printf("Connected to flespi MQTT broker\n");
    return 1;
}

void mqtt_disconnect() {
    if (is_connected) {
        MQTTClient_disconnect(client, 10000);
        MQTTClient_destroy(&client);
        is_connected = 0;
    }
}

void mqtt_publish(char* topic, char* message) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = message;
    pubmsg.payloadlen = strlen(message);
    pubmsg.qos = 1;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    MQTTClient_waitForCompletion(client, token, 10000L);
}

int main() {
    // MQTT Client ID and credentials
    if (!mqtt_connect()) {
        return 1;
    }

    // wiringPi initialization
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }
    double currentTemperature = 0.0;
    double currentTemperatureChange = 0.0;
    softPwmCreate(COOLER_PIN, 0, PWM_RANGE);
    softPwmCreate(HEATER_PIN, 0, PWM_RANGE);

    while (1) {
        
        char temp_msg[50];
        char temp_change_msg[50];
        char cooler_msg[50];
        char heater_msg[50];
        if (currentTemperature == 0.0) {
            currentTemperature = get_Temperature(SENSOR_PATH);
            snprintf(temp_msg, sizeof(temp_msg), "{\"temperature\": %.2f}", currentTemperature);
            currentTemperatureChange = 0.0;
        } else {
            currentTemperatureChange = currentTemperature - get_Temperature(SENSOR_PATH);
            currentTemperature = get_Temperature(SENSOR_PATH);
        }
        // allocate memory
        createClassifiers();

        FuzzyClassifier(currentTemperature, &TemperatureState);
        FuzzyClassifier(currentTemperatureChange, &TempChangeState);
        printf("Temperature %0.3f degC\n", currentTemperature);
        printf("Temp Change %0.3f degC/1 min \n\n", currentTemperatureChange);

        fuzzyInference(rules, (sizeof(rules) / sizeof(rules[0])));

        printf("Cooler Speed Label \n");
        printClassifier(&PelCoolerSpeed, peltierSpeedLabels);
        printf("Heater Speed Label \n");
        printClassifier(&PelHeaterSpeed, peltierSpeedLabels);

        double output_cooler = defuzzification(&PelCoolerSpeed);
        double output_heater = defuzzification(&PelHeaterSpeed);

        setPeltierCoolPower(output_cooler);
        printf("Cooler Speed: %0.3f: \n", output_cooler);
        setPeltierHeatPower(output_heater);
        printf("Heater Speed: %0.3f: \n", output_heater);

        snprintf(temp_msg, 50, "%.3f", currentTemperature);
        snprintf(temp_change_msg, 50, "%.3f", currentTemperatureChange);
        snprintf(cooler_msg, 50, "%.3f", output_cooler);
        snprintf(heater_msg, 50, "%.3f", output_heater);

        mqtt_publish(TOPIC_TEMP, temp_msg);
        mqtt_publish(TOPIC_TEMP_CHANGE, temp_change_msg);
        mqtt_publish(TOPIC_PELTIER_COOL, cooler_msg);
        mqtt_publish(TOPIC_PELTIER_HEAT, heater_msg);

        printf("\n====Published to MQTT====\n");
        printf("Temperature: %s degC \nTempCh: %s degC \nCooler: %s % \nHeater: %s % \n",
               temp_msg, temp_change_msg, cooler_msg, heater_msg);

        destroyClassifiers();

        sleep(15); // Delay 60s
    }

    mqtt_disconnect();
    return 0;
}
