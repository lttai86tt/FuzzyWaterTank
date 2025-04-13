#include "wiringPi.h"

#include <stdio.h>
#include <stdlib.h>

#define SENSOR_PATH "/sys/bus/w1/devices/28-3ce1d4434496/w1_slave"

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

int main(void) {
    // wiringPi initialization
    if (wiringPiSetupGpio() == -1) {
        printf("WiringPi setup failed!\n");
        return 1;
    }

    double currentTemperature, previousTemperature = -1.0;
    double currentTemperatureChange;

    while (1) {
        currentTemperature = get_Temperature(SENSOR_PATH);
        if (currentTemperature != -1) {
            writeLog("Current Temperature", currentTemperature);
            if (previousTemperature != -1) {
                currentTemperatureChange =
                    currentTemperature - previousTemperature;
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
            currentTemperatureChange =
                currentTemperature - get_Temperature(SENSOR_PATH);
            currentTemperature = get_Temperature(SENSOR_PATH);
        }

        printf("Temperature %0.3f degC\n", currentTemperature);
        printf("Temp Change %0.3f degC/30s \n\n", currentTemperatureChange);

        sleep(30);
    }
    return 0;
}