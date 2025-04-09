/**
 * @file PeltierControl.c
 *
 * To-Do:
 * 1 . testing mqtt broker
 */

#include "MQTTClient.h"
#include "fuzzyc.h"
#include "softPwm.h"
#include "unistd.h"
#include "wiringPi.h"

#include <hiredis/hiredis.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sensor and PWM
#define PWM_RANGE 70
#define COOLER_PIN 23
#define HEATER_PIN 24
#define SENSOR_PATH "/sys/bus/w1/devices/28-3ce1d4434496/w1_slave"

// MQTT Broker
#define MQTT_ADDRESS                                                           \
    "ssl://eab5d59939f54918ad1cc5129f7a2fd8.s1.eu.hivemq.cloud:8883"
#define MQTT_CLIENTID "WaterTank"
#define USERNAME "WaterTank_Pi"
#define PASSWORD "Pi123123"
#define QOS 1
#define TIMEOUT 10000L

#define TOPIC_TEMP "watertank/temp"
#define TOPIC_TEMP_CHANGE "watertank/temp_change"
#define TOPIC_PELTIER_COOL "watertank/cooler"
#define TOPIC_PELTIER_HEAT "watertank/heater"

// Define Redis server address and port
#define REDIS_ADDRESS "172.30.85.87" // Replace with your Redis server address
#define REDIS_PORT 6379              // Default Redis port
// Define the labels for the fuzzy sets (only used for debugging)
const char *tempLabels[] = {"Cool", "Normal",
                            "Hot"}; // Lanh <> Binh thuong <> Nong
const char *changeLabels[] = {"Dec", "Stable",
                              "Inc"}; // Giam <> On dinh <> Tang
const char *peltierSpeedLabels[] = {"Off", "Slow", "Medium", "Fast"};

// Define the input fuzzy sets
FuzzySet_t TemperatureState; // Trang thai nhiet do
FuzzySet_t TempChangeState;  // Trang thai thay doi nhiet do

// Define the output fuzzy set
FuzzySet_t PelCoolerSpeed; // Toc do cua may lam mat
FuzzySet_t PelHeaterSpeed; // Toc do cua may lam nong
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

/*
=========> X(TemChange is error)
*/

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

int main() {
    // MQTT Client ID and credentials
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    int rc;

    MQTTClient_create(&client, MQTT_ADDRESS, MQTT_CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.ssl = &ssl_opts;

    // Kết nối đến HiveMQ
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Connected to MQTT failed, error %d\n", rc);
        return 1;
    }
    printf("Successfully connected to HiveMQ\n");

    // Initialize Redis connection
    redisContext *redis_conn = redisConnect(REDIS_ADDRESS, REDIS_PORT);
    if (redis_conn == NULL || redis_conn->err) {
        if (redis_conn) {
            printf("Connection to Redis failed: %s\n", redis_conn->errstr);
            redisFree(redis_conn);
        } else {
            printf(
                "Connection to Redis failed: can't allocate redis context\n");
        }
        MQTTClient_disconnect(client, 10000);
        MQTTClient_destroy(&client);
        return 1;
    }
    printf("Successfully connected to Redis\n");

    // wiringPi initialization
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        MQTTClient_disconnect(client, 10000);
        MQTTClient_destroy(&client);
        redisFree(redis_conn);
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
            currentTemperatureChange = 0.0;
        } else {
            currentTemperatureChange =
                currentTemperature - get_Temperature(SENSOR_PATH);
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
        setPeltierCoolPower(output_cooler);
        printf("Cooler Speed: %0.3f: \n", output_cooler);
        setPeltierHeatPower(output_heater);
        printf("Heater Speed: %0.3f: \n", output_heater);

        printf("\n====MQTT publisher====\n");
        snprintf(temp_msg, sizeof(temp_msg), "%.3f", currentTemperature);
        MQTTClient_publish(client, TOPIC_TEMP, strlen(temp_msg), temp_msg, QOS,
                           0, NULL);
        printf("watertank/temp: %s degC\n", temp_msg);
        snprintf(temp_change_msg, sizeof(temp_change_msg), "%.3f",
                 currentTemperatureChange);
        MQTTClient_publish(client, TOPIC_TEMP_CHANGE, strlen(temp_change_msg),
                           temp_change_msg, QOS, 0, NULL);
        printf("watertank/temp_change: %s degC\n", temp_change_msg);
        snprintf(cooler_msg, sizeof(cooler_msg), "%.3f", output_cooler);
        MQTTClient_publish(client, TOPIC_PELTIER_COOL, strlen(cooler_msg),
                           cooler_msg, QOS, 0, NULL);
        printf("watertank/cooler: %s %\n", cooler_msg);
        snprintf(heater_msg, sizeof(heater_msg), "%.3f", output_heater);
        MQTTClient_publish(client, TOPIC_PELTIER_HEAT, strlen(heater_msg),
                           heater_msg, QOS, 0, NULL);
        printf("watertank/heater: %s %\n", heater_msg);

        printf("\n====Redis publisher====\n");
        redisReply *reply;

        // Store temperature in Redis
        reply = redisCommand(redis_conn, "SET watertank:temp %s", temp_msg);
        if (reply == NULL) {
            printf("Redis command failed (SET watertank:temp): %s\n",
                   redis_conn->errstr);
        } else {
            printf("Redis: watertank:temp -> %s\n", reply->str);
            freeReplyObject(reply);
        }

        // Store temperature change in Redis
        reply = redisCommand(redis_conn, "SET watertank:temp_change %s",
                             temp_change_msg);
        if (reply == NULL) {
            printf("Redis command failed (SET watertank:temp_change): %s\n",
                   redis_conn->errstr);
        } else {
            printf("Redis: watertank:temp_change -> %s\n", reply->str);
            freeReplyObject(reply);
        }

        // Store cooler speed in Redis
        reply = redisCommand(redis_conn, "SET watertank:cooler %s", cooler_msg);
        if (reply == NULL) {
            printf("Redis command failed (SET watertank:cooler): %s\n",
                   redis_conn->errstr);
        } else {
            printf("Redis: watertank:cooler -> %s\n", reply->str);
            freeReplyObject(reply);
        }

        // Store heater speed in Redis
        reply = redisCommand(redis_conn, "SET watertank:heater %s", heater_msg);
        if (reply == NULL) {
            printf("Redis command failed (SET watertank:heater): %s\n",
                   redis_conn->errstr);
        } else {
            printf("Redis: watertank:heater -> %s\n", reply->str);
            freeReplyObject(reply);
        }

        destroyClassifiers();

        sleep(30); // Delay 60s
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    redisFree(redis_conn);
    return 0;
}
