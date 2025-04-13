/**
 * @file PeltierControl.c
 */

#include "unistd.h"
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuzzyc.h"
#include "MQTTClient.h"
#include "softPwm.h"
#include "wiringPi.h"

// Sensor and PWM
#define PWM_RANGE   80
#define COOLER_PIN  23
#define HEATER_PIN  24
#define SENSOR_PATH "/sys/bus/w1/devices/28-3ce1d4434496/w1_slave"

// MQTT Broker
#define MQTT_ADDRESS     "ssl://eab5d59939f54918ad1cc5129f7a2fd8.s1.eu.hivemq.cloud:8883"
#define MQTT_CLIENTID    "WaterTank"
#define USERNAME         "WaterTank_Pi"  
#define PASSWORD         "Pi123123"    
#define QOS              1

#define TOPIC_TEMP              "watertank/temperature"
#define TOPIC_TEMP_CHANGE       "watertank/temperature_change"
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

// Write log to file
void writeLog(const char *message, float parameter) {
    FILE *f = fopen("Water_Tank_Report.txt", "a"); // Open the file in append mode
    if (f == NULL) {
        printf("Can not open the file log.\n");
        return;
    }

    // Take the current time
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    char time_str[30];
    strftime(time_str, 30, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(f, "[%s] %s - %.2f\n", time_str, message, parameter);
    fclose(f);
}

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
    X(TEMPERATURE_LOW, -10.0, 10.0, 25.0, 30.0, TRAPEZOIDAL)                    \
    X(TEMPERATURE_MEDIUM, 28.0, 30.0, 32.0, TRIANGULAR)                        \
    X(TEMPERATURE_HIGH, 30.0, 38.0, 100.0, 100.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TemperatureMembershipFunctions)

#define TempChangeMembershipFunctions(X)                                       \
    X(TEMP_CHANGE_DECREASING, -20.0, -10.0, -1.0, 0.0, TRAPEZOIDAL)            \
    X(TEMP_CHANGE_STABLE, -1.0, 0.0, 1.0, TRIANGULAR)                          \
    X(TEMP_CHANGE_INCREASING, 0.0, 1.0, 10.0, 20.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TempChangeMembershipFunctions)
//
#define PeltierCoolerSpeedMembershipFunctions(X)                               \
    X(PELTIER_COOLER_SPEED_OFF, -10.0, 0.0, 0.0, 0.0, TRAPEZOIDAL)             \
    X(PELTIER_COOLER_SPEED_SLOW, 00.0, 15.0, 30.0, 40.0, TRAPEZOIDAL)          \
    X(PELTIER_COOLER_SPEED_MEDIUM, 30.0, 40.0, 55.0, 70.0, TRAPEZOIDAL)        \
    X(PELTIER_COOLER_SPEED_FAST, 55.0, 70.0, 80.0, 80.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(PeltierCoolerSpeedMembershipFunctions)

#define PeltierHeaterSpeedMembershipFunctions(X)                               \
    X(PELTIER_HEATER_SPEED_OFF, -10.0, 0.0, 0.0, 0.0, TRAPEZOIDAL)             \
    X(PELTIER_HEATER_SPEED_SLOW, 00.0, 15.0, 30.0, 40.0, TRAPEZOIDAL)          \
    X(PELTIER_HEATER_SPEED_MEDIUM, 30.0, 40.0, 55.0, 70.0, TRAPEZOIDAL)        \
    X(PELTIER_HEATER_SPEED_FAST, 55.0, 70.0, 80.0, 80.0, TRAPEZOIDAL)
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

    MQTTClient_create(&client, MQTT_ADDRESS, MQTT_CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.ssl = &ssl_opts;

    // Connecting to HiveMQ
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Connected to MQTT failed, error %d\n", rc);
        return 1;
    }
    printf("Successfully connected to HiveMQ\n");

    // wiringPi initialization
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }

    double currentTemperature, previousTemperature = -1.0;
    double currentTemperatureChange;
    softPwmCreate(COOLER_PIN, 0, PWM_RANGE);
    softPwmCreate(HEATER_PIN, 0, PWM_RANGE);

    while (1) {
        char temp_msg[50], temp_change_msg[50], cooler_msg[50], heater_msg[50];

        currentTemperature = get_Temperature(SENSOR_PATH);
        if (currentTemperature != -1) { 
            writeLog("Current Temperature", currentTemperature); 
            if (previousTemperature != -1) {
                currentTemperatureChange = currentTemperature - previousTemperature;
                writeLog("Temperature Change", currentTemperatureChange);
            } else {
                currentTemperatureChange = 0.0;
                writeLog("Temperature Change", currentTemperatureChange);
            }
            previousTemperature = currentTemperature; 
        } else {
            printf("Can not read a temperature sensor.\n");
            writeLog("Error read a temperature sensor", currentTemperature);
        }
        if (currentTemperature == 0.0) {
            currentTemperature = get_Temperature(SENSOR_PATH);
            currentTemperatureChange = 0.0;
        } else if (currentTemperature != 0.0) {
            currentTemperatureChange = currentTemperature - get_Temperature(SENSOR_PATH);
            currentTemperature = get_Temperature(SENSOR_PATH);
        }
        // allocate memory
        createClassifiers();

        // Set the input values for the fuzzy classifiers
        FuzzyClassifier(currentTemperature, &TemperatureState);
        FuzzyClassifier(currentTemperatureChange, &TempChangeState);

        // Print the input values and their fuzzy membership values
        printf("Temperature %0.3f degC\n", currentTemperature);
        printf("Temp Change %0.3f degC/30s \n\n", currentTemperatureChange);

        fuzzyInference(rules, (sizeof(rules) / sizeof(rules[0])));

        printf("Cooler Speed Label \n");
        printClassifier(&PelCoolerSpeed, peltierSpeedLabels);
        printf("Heater Speed Label \n");
        printClassifier(&PelHeaterSpeed, peltierSpeedLabels);

        double output_cooler = defuzzification(&PelCoolerSpeed);
        double output_heater = defuzzification(&PelHeaterSpeed);

        setPeltierCoolPower(output_cooler);
        printf("Cooler Speed: %0.3f: \n", output_cooler);
        writeLog("Cooler Speed", output_cooler);
        setPeltierHeatPower(output_heater);
        printf("Heater Speed: %0.3f: \n", output_heater);
        writeLog("Heater Speed", output_heater);

        printf("\n====MQTT publisher====\n");

        snprintf(temp_msg, sizeof(temp_msg), "{%.3f}", currentTemperature);
        MQTTClient_publish(client, TOPIC_TEMP, strlen(temp_msg), temp_msg, QOS, 0, NULL);
        printf("watertank/temperature: %s degC\n", temp_msg);

        snprintf(temp_change_msg, sizeof(temp_change_msg), "{%.3f}", currentTemperatureChange);
        MQTTClient_publish(client, TOPIC_TEMP_CHANGE, strlen(temp_change_msg), temp_change_msg, QOS, 0, NULL);
        printf("watertank/temperature_change: %s degC\n", temp_change_msg);

        snprintf(cooler_msg, sizeof(cooler_msg), "{%.3f}", output_cooler);
        MQTTClient_publish(client, TOPIC_PELTIER_COOL, strlen(cooler_msg), cooler_msg, QOS, 0, NULL);
        printf("watertank/cooler: %s %\n", cooler_msg);
        snprintf(heater_msg, sizeof(heater_msg), "{%.3f}", output_heater);
        MQTTClient_publish(client, TOPIC_PELTIER_HEAT, strlen(heater_msg), heater_msg, QOS, 0, NULL);
        printf("watertank/heater: %s %\n", heater_msg);       

        destroyClassifiers();

        sleep(30); // Sleep for 30 seconds before the next reading
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
