/**
 * @file PeltierControl.c
 *
 * To-Do:  3 Days
 * 1. Define the fuzzy sets for the input and output
 * 2. Define the fuzzy rules for the system
 * 3. Configuration Control Output PWM for Peltier Cooler and Heater
 */

#include "fuzzyc.h"

#include "unistd.h"
#include "wiringPi.h"
#include <math.h>
#include "softPwm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PWM_RANGE    70
#define COOLER_PIN   23
#define HEATER_PIN   24
#define SENSOR_PATH  "/sys/bus/w1/devices/28-3ce1d4434496/w1_slave"

// Define the labels for the fuzzy sets (only used for debugging)

const char *tempLabels[] = {"Cool", "Normal",
                            "Hot"}; // Lanh <> Binh thuong <> Nong
const char *changeLabels[] = {"Dec", "Stable",
                              "Inc"}; // Giam <> On dinh <> Tang
const char *peltierSpeedLabels[] = {"Off", "Slow", "Medium", "Fast"};

const char *fanLabels[] = {"Off", "On"};                    // UNUSED
const char *lmhLabels[] = {"Off", "Low", "Medium", "High"}; // UNUSED

// Define the input fuzzy sets
FuzzySet_t TemperatureState; // Trang thai nhiet do
FuzzySet_t TempChangeState;  // Trang thai thay doi nhiet do

// Define the output fuzzy set
FuzzySet_t PelCoolerSpeed; // Toc do cua may lam mat
FuzzySet_t PelHeaterSpeed; // Toc do cua may lam nong

// Helper function to map a value from one range to another
double map_range(double value, double in_min, double in_max, double out_min,
                 double out_max) {
    double mapped =
        (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    return fmin(fmax(mapped, out_min), out_max);
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

void setPeltierHeatPower(int heaterPower){
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
    X(TEMPERATURE_LOW, 0.0, 18.0, 24.0, TRIANGULAR)                      \
    X(TEMPERATURE_MEDIUM, 18.0, 24.0, 35.0, TRIANGULAR)                   \
    X(TEMPERATURE_HIGH, 24.0, 35.0, 100.0, 100.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TemperatureMembershipFunctions)

#define TempChangeMembershipFunctions(X)                                       \
    X(TEMP_CHANGE_DECREASING, -20.0, -20.0, -2.0, 0.0, TRAPEZOIDAL)                 \
    X(TEMP_CHANGE_STABLE, -2.0, 0.0, 2.0, 0.0, TRIANGULAR)                      \
    X(TEMP_CHANGE_INCREASING, 0.0, 2.0, 20.0, 20.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(TempChangeMembershipFunctions)
//
#define PeltierCoolerSpeedMembershipFunctions(X)                               \
    X(PELTIER_COOLER_SPEED_OFF, -20.0, -10.0, 0.0, 10.0, RECTANGULAR)           \
    X(PELTIER_COOLER_SPEED_SLOW, 00.0, 7.0, 15.0, 25.0, TRAPEZOIDAL)          \
    X(PELTIER_COOLER_SPEED_MEDIUM, 15.0, 25.0, 30.0, 40.0, TRAPEZOIDAL)        \
    X(PELTIER_COOLER_SPEED_FAST, 35.0, 45.0, 70.0, 70.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(PeltierCoolerSpeedMembershipFunctions)

#define PeltierHeaterSpeedMembershipFunctions(X)                               \
    X(PELTIER_HEATER_SPEED_OFF, -20.0, -10.0, 0.0, 0.0, RECTANGULAR)           \
    X(PELTIER_HEATER_SPEED_SLOW, 00.0, 7.0, 15.0, 25.0, TRAPEZOIDAL)          \
    X(PELTIER_HEATER_SPEED_MEDIUM, 15.0, 25.0, 30.0, 40.0, TRAPEZOIDAL)        \
    X(PELTIER_HEATER_SPEED_FAST, 35.0, 45.0, 70.0, 70.0, TRAPEZOIDAL)
DEFINE_FUZZY_MEMBERSHIP(PeltierHeaterSpeedMembershipFunctions)
// Define the fuzzy rules
/*
    >> NEED TO DEFINE THE RULES FOR THE SYSTEM <<
*/
FuzzyRule_t rules[] = {

    // Rule 1:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_FAST)),

    // Rule 2:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),

    // Rule 3:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_HIGH)),
                     ANY_OF(VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_MEDIUM)),

    // Rule 4:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_DECREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_SLOW)),
    // Rule 5:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelCoolerSpeed, PELTIER_COOLER_SPEED_SLOW)),
    // Rule 6:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_MEDIUM),
                            VAR(TempChangeState, TEMP_CHANGE_STABLE))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_OFF)),
    // Rule 7:
    PROPOSITION(WHEN(ALL_OF(VAR(TemperatureState, TEMPERATURE_LOW),
                            VAR(TempChangeState, TEMP_CHANGE_INCREASING))),
                THEN(PelHeaterSpeed, PELTIER_HEATER_SPEED_MEDIUM)),
};

// Helper function to create the fuzzy classifiers
void createClassifiers() {
    FuzzySetInit(&TemperatureState, TemperatureMembershipFunctions,
                 FUZZY_LENGTH(TemperatureMembershipFunctions));
    FuzzySetInit(&TempChangeState, TempChangeMembershipFunctions,
                 FUZZY_LENGTH(TempChangeMembershipFunctions));

    //
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

int main(int argc, char *argv[]) {

    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }
    double currentTemperature = 0.0;
    double currentTemperatureChange = 0.0;
    softPwmCreate(COOLER_PIN, 0, PWM_RANGE);
    softPwmCreate(HEATER_PIN, 0, PWM_RANGE);

    while (1) {

        if (currentTemperature == 0.0) {
            currentTemperature = get_Temperature(SENSOR_PATH);
            currentTemperatureChange = 0.0;
        } else {
            currentTemperatureChange = currentTemperature - get_Temperature(SENSOR_PATH);
            currentTemperature = get_Temperature(SENSOR_PATH);
        }

        if (argc <= 2) {
            printf(
                "Usage: %s <currentTemperature> <currentTemperatureChange>\n",
                argv[0]);
            return 1;
        } else {
            currentTemperature = atof(argv[1]);
            currentTemperatureChange = atof(argv[2]);
        }
        FuzzyClassifier(currentTemperature, &TemperatureState);
        FuzzyClassifier(currentTemperatureChange, &TempChangeState);
        printf("Temperature %.04f degC\n", currentTemperature);
        printf("Temp Change %.04f degC/30sec\n", currentTemperatureChange);

        fuzzyInference(rules, (sizeof(rules) / sizeof(rules[0])));

        printClassifier(&PelCoolerSpeed, peltierSpeedLabels);
        printClassifier(&PelHeaterSpeed, peltierSpeedLabels);

        double output_cooler = defuzzification(&PelCoolerSpeed);
        double output_heater = defuzzification(&PelHeaterSpeed);

        setPeltierCoolPower(output_cooler);
        printf("Cooler Speed: %0.4f: \n", output_cooler);
        setPeltierHeatPower(output_heater);
        printf("Heater Speed: %0.4f: \n", output_heater);

        FuzzySetFree(&TemperatureState);
        FuzzySetFree(&TempChangeState);
        FuzzySetFree(&PelHeaterSpeed);
        FuzzySetFree(&PelCoolerSpeed);

        destroyClassifiers();

        delay(15000); // Delay 30s
        // vTaskDelay(30000 / portTICK_PERIOD_MS);   //Delay 30s
    }
    return 0;
}
