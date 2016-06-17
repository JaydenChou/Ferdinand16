/*  Copyright (C) 2016  Adam Green (https://github.com/adamgreen)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
/* Test harness for my heading class. */
#include <assert.h>
#include <mbed.h>
#include "ConfigFile.h"
#include "DmaSerial.h"
#include "FlashFileSystem.h"
#include "files.h"
#include "Sparkfun9DoFSensorStick.h"


static volatile bool g_resetRequested = false;

#if !MRI_ENABLE
static DmaSerial g_serial(USBTX, USBRX);
#endif // !MRI_ENABLE


// Function prototypes.
void serialReceiveISR();


int main()
{
    static Timer timer;
    timer.start();
#if !MRI_ENABLE
    g_serial.baud(230400);
    g_serial.attach(serialReceiveISR);
#endif // !MRI_ENABLE

    static FlashFileSystem fileSystem("flash", g_fileSystemData);
    if (!fileSystem.IsMounted())
        error("Encountered error mounting FLASH file system.\n");

    static ConfigFile configFile;
    if (configFile.open("/flash/config.ini"))
        error("Encountered error opening /flash/config.ini\n");

    static SensorCalibration calibration;
    if (!configFile.getIntVector("compass.accelerometer.min", &calibration.accelMin))
        error("Failed to read compass.accelerometer.min\n");
    if (!configFile.getIntVector("compass.accelerometer.max", &calibration.accelMax))
        error("Failed to read compass.accelerometer.max\n");
    if (!configFile.getIntVector("compass.magnetometer.min", &calibration.magMin))
        error("Failed to read compass.magnetometer.min\n");
    if (!configFile.getIntVector("compass.magnetometer.max", &calibration.magMax))
        error("Failed to read compass.magnetometer.max\n");
    if (!configFile.getFloatVector("compass.gyro.coefficient.A", &calibration.gyroCoefficientA))
        error("Failed to read compass.gyro.coefficient.A\n");
    if (!configFile.getFloatVector("compass.gyro.coefficient.B", &calibration.gyroCoefficientB))
        error("Failed to read compass.gyro.coefficient.B\n");
    if (!configFile.getFloatVector("compass.gyro.scale", &calibration.gyroScale))
        error("Failed to read compass.gyro.scale\n");
    configFile.close();

    static Sparkfun9DoFSensorStick sensorStick(p9, p10, &calibration);
    if (sensorStick.didInitFail())
        error("Encountered I2C I/O error during Sparkfun 9DoF Sensor Stick init.\n");

    for (;;)
    {
        char buffer[256];
        int  length;

        SensorValues sensorValues = sensorStick.getRawSensorValues();
        if (sensorStick.didIoFail())
            error("Encountered I2C I/O error during fetch of Sparkfun 9DoF Sensor Stick readings.\n");
        SensorCalibratedValues calibratedValues = sensorStick.calibrateSensorValues(&sensorValues);

        int elapsedTime = timer.read_us();
        timer.reset();
        length = snprintf(buffer, sizeof(buffer), "%s%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.1f,%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
                          g_resetRequested ? "R," : "",
                          sensorValues.accel.x, sensorValues.accel.y, sensorValues.accel.z,
                          sensorValues.mag.x, sensorValues.mag.y, sensorValues.mag.z,
                          sensorValues.gyro.x, sensorValues.gyro.y, sensorValues.gyro.z,
                          sensorValues.gyroTemperature,
                          elapsedTime,
                          sensorStick.getIdleTimePercent(),
                          calibratedValues.accel.x, calibratedValues.accel.y, calibratedValues.accel.z,
                          calibratedValues.mag.x, calibratedValues.mag.y, calibratedValues.mag.z,
                          calibratedValues.gyro.x, calibratedValues.gyro.y, calibratedValues.gyro.z);
        assert( length < (int)sizeof(buffer) );
        g_resetRequested = false;

#if MRI_ENABLE
        printf("%s", buffer);
#else
        g_serial.dmaTransmit(buffer, length);
#endif
    }

    return 0;
}

#if !MRI_ENABLE
void serialReceiveISR()
{
    while (g_serial.readable())
    {
        int byte = g_serial.getc();
        if (byte == 'R')
        {
            g_resetRequested = true;
        }
    }
}
#endif // !MRI_ENABLE