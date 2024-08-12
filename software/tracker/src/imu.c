#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"

#define I2C_ADDR_BNO055  (0x28)

#define MAGNETIC_DEVIATION_OFFSET   (+0.6) // Approximate average for South UK

void *imu_thread(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  int h = i2cOpen(3, I2C_ADDR_BNO055, 0);
  if(h < 0)
  {
    fprintf(stderr, "Error: I2C failed to initialise: %d\n", h);
    app_state->app_exit = true;
    return NULL;
  }

  int8_t chip_id = i2cReadByteData(h, 0x00);

  if((chip_id & 0xFF) >= 0)
  {
    printf("IM Chip ID: 0x%02x (correct = a0)\n", (chip_id & 0xFF));
  }
  else
  {
    printf("Error: Read of Chip ID failed: %d (0x%02x)\n", chip_id, chip_id);
  }

  // Reset chip
  //i2cWriteByteData(h, 0x35, (1 << 7) | (1 << 6));
  // Wait for chip reboot
  //sleep(1);

  // Check OPR_MODE
  int8_t opr_mode = i2cReadByteData(h, 0x3D);
  //printf("OPR Mode: 0x%02x\n", opr_mode & 0xF);
  if((opr_mode & 0xF) != 0xC)
  {
    // Set fusion mode (NDOF)
    i2cWriteByteData(h, 0x3D, opr_mode | 0xC);
    sleep(1);
    opr_mode = i2cReadByteData(h, 0x3D);
    //printf("OPR Mode: 0x%02x\n", opr_mode & 0xF);
  }

  // Set Euler Units to degrees
  int8_t reg = i2cReadByteData(h, 0x3B);
  i2cWriteByteData(h, 0x3B, reg | (1 << 3));

  int8_t calib_status;
  char buf[6];
  float heading_degrees;
  float roll_degrees;
  float pitch_degrees;


  while(!(app_state->app_exit))
  {
    // Poll Calib_Stat to check calibration status
    calib_status = i2cReadByteData(h, 0x35);

    //printf("Sys Calib Status: 0x%01x\n", (calib_status & 0xC0) >> 6);
    //printf("Gyr Calib Status: 0x%01x\n", (calib_status & 0x30) >> 4);
    //printf("Acc Calib Status: 0x%01x\n", (calib_status & 0x0C) >> 2);
    //printf("Mag Calib Status: 0x%01x\n", calib_status & 0x03);

    // Read Heading + Pitch + Roll
    i2cReadI2CBlockData(h, 0x1A, buf, 6);

    heading_degrees = (float)((int16_t)buf[0] | ((int16_t)buf[1] << 8)) / 16.0;
    roll_degrees = (float)((int16_t)((int8_t)buf[2]) | ((int16_t)((int8_t)buf[3]) << 8)) / 16.0;
    pitch_degrees = (float)((int16_t)((int8_t)buf[4]) | ((int16_t)((int8_t)buf[5]) << 8)) / 16.0;

    //printf("Heading: %+03.1f\n", heading_degrees);
    //printf("Roll:    %+03.1f\n", roll_degrees);
    //printf("Pitch:   %+03.1f\n", pitch_degrees);

    app_state->imu_heading = heading_degrees;
    app_state->imu_heading_calstatus = calib_status & 0x03;
    app_state->imu_roll = roll_degrees;
    app_state->imu_pitch = pitch_degrees;

    app_state->imu_valid = true;

    if((calib_status & 0x03) == 3)
    {
      app_state->compass_heading = heading_degrees + MAGNETIC_DEVIATION_OFFSET;
      app_state->compass_valid = true;
    }
    else
    {
      app_state->compass_valid = false;
    }

    usleep(200*1000); // 5 Hz
  }

  i2cClose(h);

  printf("IMU thread exiting..\n");

  return NULL;
}
