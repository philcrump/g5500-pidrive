#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#define I2C_ADDR_BNO055  (0x28)

static bool app_exit = false;
static void sigint_handler(int sig)
{
  (void)sig;
  app_exit = true;
}

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  if(gpioInitialise() < 0)
  {
    fprintf(stderr, "Error: gpio failed to initialise\n");
    return 1;
  }

  /* Override pigpio handler */
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);

  printf("pigpio version %d.\n", gpioVersion());
  printf("Hardware revision %d.\n", gpioHardwareRevision());

  int h = i2cOpen(3, I2C_ADDR_BNO055, 0);

  if(h < 0)
  {
    fprintf(stderr, "Error: I2C failed to initialise: %d\n", h);
    return 1;
  }

  int8_t chip_id = i2cReadByteData(h, 0x00);

  if((chip_id & 0xFF) >= 0)
  {
    printf("Chip ID: 0x%02x (correct = a0)\n", (chip_id & 0xFF));
  }
  else
  {
    printf("Error: Read of Chip ID failed: %d (0x%02x)\n", chip_id, chip_id);
  }

  // Reset chip
  i2cWriteByteData(h, 0x35, (1 << 7) | (1 << 6));
  // Wait for chip reboot
  sleep(1);

  // Check OPR_MODE
  int8_t opr_mode = i2cReadByteData(h, 0x3D);
  printf("OPR Mode: 0x%02x\n", opr_mode & 0xF);
  // Set fusion mode (NDOF)
  i2cWriteByteData(h, 0x3D, opr_mode | 0xC);
  sleep(1);
  opr_mode = i2cReadByteData(h, 0x3D);
  printf("OPR Mode: 0x%02x\n", opr_mode & 0xF);

  // Set Euler Units to degrees
  int8_t units = i2cReadByteData(h, 0x3B);
  i2cWriteByteData(h, 0x3B, units | (1 << 3));

  int8_t calib_status;
  char buf[6];
  float heading_degrees;
  float roll_degrees;
  float pitch_degrees;
  while(!app_exit)
  {
    printf("\n");

    // Poll Calib_Stat to check calibration status
    calib_status = i2cReadByteData(h, 0x35);

    printf("Sys Calib Status: 0x%01x\n", (calib_status & 0xC0) >> 6);
    printf("Gyr Calib Status: 0x%01x\n", (calib_status & 0x30) >> 4);
    printf("Acc Calib Status: 0x%01x\n", (calib_status & 0x0C) >> 2);
    printf("Mag Calib Status: 0x%01x\n", calib_status & 0x03);

    // Read Heading + Pitch + Roll
    i2cReadI2CBlockData(h, 0x1A, buf, 6);

    heading_degrees = (float)((int16_t)buf[0] | ((int16_t)buf[1] << 8)) / 16.0;
    roll_degrees = (float)((int16_t)((int8_t)buf[2]) | ((int16_t)((int8_t)buf[3]) << 8)) / 16.0;
    pitch_degrees = (float)((int16_t)((int8_t)buf[4]) | ((int16_t)((int8_t)buf[5]) << 8)) / 16.0;

    printf("Heading: %+03.1f\n", heading_degrees);
    printf("Roll:    %+03.1f\n", roll_degrees);
    printf("Pitch:   %+03.1f\n", pitch_degrees);

    sleep(1);
  }


  i2cClose(h);

  gpioTerminate();

  return 0;
}
