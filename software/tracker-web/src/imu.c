#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"
#include "util/timing.h"

#define I2C_ADDR_BNO055  (0x28)

void bno055_setup(int h)
{
  //Reset chip
  i2cWriteByteData(h, 0x3F, (1 << 5));

  //Wait for chip reboot
  sleep_ms(1000);

  // Set Register Page -> 0
  i2cWriteByteData(h, 0x07, 0x00);

  int8_t chip_id = i2cReadByteData(h, 0x00);
  if((chip_id & 0xFF) == 0xa0)
  {
    printf("IMU Chip ID: 0x%02x (correct = a0)\n", (chip_id & 0xFF));
  }
  else
  {
    printf("IMU Error: Read of Chip ID failed: %d (0x%02x)\n", chip_id, chip_id);
    pthread_exit(NULL);
  }

  // Set CONFIGMODE
  int8_t opr_mode = i2cReadByteData(h, 0x3D);
  i2cWriteByteData(h, 0x3D, (opr_mode & 0xF0) | 0x0);

  sleep_ms(20);

  // Change Register Page 0 -> 1
  i2cWriteByteData(h, 0x07, 0x01);

  // Set ACC_CONFIG: Normal, 7.8Hz, 2G
  i2cWriteByteData(h, 0x08, 0x00);

  // Set MAG_CONFIG: Normal, High Accuracy, 2Hz
  i2cWriteByteData(h, 0x08, 0x18);

  // Change Register Page 1 -> 0
  i2cWriteByteData(h, 0x07, 0x00);

  // Set ACCMAG mode
  opr_mode = i2cReadByteData(h, 0x3D);
  i2cWriteByteData(h, 0x3D, (opr_mode & 0xF0) | 0x4);

  opr_mode = i2cReadByteData(h, 0x3D);
  //printf("OPR Mode: 0x%02x\n", opr_mode & 0xF);

  sleep_ms(20);

  //int8_t status = i2cReadByteData(h, 0x39);
  //printf("Mag: Chip status: 0x%02x\n", status);

  sleep_ms(100);
}

void *loop_imu(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  int r = 0;

  int h = i2cOpen(3, I2C_ADDR_BNO055, 0);
  if(h < 0)
  {
    fprintf(stderr, "IMU Error: I2C failed to initialise: %d\n", h);
    app_state->app_exit = true;
    pthread_exit(NULL);
  }

  bno055_setup(h);

  int16_t mag_x, mag_y; //, mag_z;

  char buf[12];

  while(!(app_state->app_exit))
  {
    r = i2cReadI2CBlockData(h, 0x08, buf, 12);
    while(r < 0)
    {
      printf("Mag:  I2C read error: %d\n", r);
      sleep_ms(10);

      r = i2cReadI2CBlockData(h, 0x08, buf, 12);
    }

    mag_x = (buf[7] << 8) | buf[6];
    mag_y = (buf[9] << 8) | buf[8];
    //mag_z = (buf[11] << 8) | buf[10];

#if 1
    if(mag_x == 0 && mag_y == 0)
    {
      //printf("Mag: Caught 0,0\n");
      // Check system status
      r = i2cReadI2CBlockData(h, 0x39, buf, 2);

      if(buf[0] == 0x01)
      {
        printf(" - System Error: %02x\n", buf[1]);
        bno055_setup(h);
      }
    }
#endif

    app_state->mag_x = mag_x;
    app_state->mag_y = mag_y;

    app_state->mag_valid = true;
    app_state->mag_lastupdated_ms = timestamp_ms();

    //printf("Mag X: %+05d, Mag Y: %+05d\n", mag_x, mag_y);
    //printf("%+05d,%+05d,%.1f\n", mag_x, mag_y, app_state->encoder_az_deg);


    sleep_ms(90);
  }

  i2cClose(h);

  printf("IMU thread exiting..\n");

  pthread_exit(NULL);
}
