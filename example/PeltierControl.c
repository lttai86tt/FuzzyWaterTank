/**
 * @file PeltierControl.c
 *
 * To-Do:  
 * 1 . testing mqtt broker
 */

#include "fuzzyc.h"
#include "mosquitto.h"
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
#define MQTT_HOST   ""  //"tcp://broker.hivemq.com:1883"
#define CLIENTID    "WaterTank_Publisher"
#define QOS         1
#define MQTT_PORT   1883
#define TIMEOUT     10000L

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

void publish_message(struct mosquitto *mosq, const char *topic, const char *message) {
    int ret = mosquitto_publish(mosq, NULL, topic, strlen(message), message, 1, false);
    if (ret != MOSQ_ERR_SUCCESS) {
        printf("Send message %s failed: %s\n", topic, mosquitto_strerror(ret));
    } else {
        printf("Sent : [%s] %s\n", topic, message);
    }
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

int main() {
    // MQTT initialization
    struct mosquitto *mosq;
    mosquitto_lib_init();
    mosq = mosquitto_new("Publisher", true, NULL);
    if (!mosq) {
        printf("Error: Can not initialization mosquitto client\n");
        return 1;
    }

    // Connected to MQTT Broker
    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60)) {
        printf("Error: Can not connect to MQTT broker\n");
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
            currentTemperatureChange = 0.0;
        } else {
            currentTemperatureChange = currentTemperature - get_Temperature(SENSOR_PATH);
            currentTemperature = get_Temperature(SENSOR_PATH);
        }

        sprintf(temp_msg, "%.2f", currentTemperature);
        sprintf(temp_msg, "%.2f", currentTemperatureChange);
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
        sprintf(cooler_msg, "%d", output_cooler);
        printf("Cooler Speed: %0.3f: \n", output_cooler);
        setPeltierHeatPower(output_heater);
        sprintf(heater_msg, "%d", output_heater);
        printf("Heater Speed: %0.3f: \n", output_heater);
        
        // Gửi dữ liệu lên MQTT 
        publish_message(mosq, TOPIC_TEMP, temp_msg);
        publish_message(mosq, TOPIC_TEMP_CHANGE, temp_change_msg);
        publish_message(mosq, TOPIC_PELTIER_HEAT, heater_msg);
        publish_message(mosq, TOPIC_PELTIER_COOL, cooler_msg);

        destroyClassifiers();

        sleep(60); // Delay 60s
    }

    //Clean memory
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}
