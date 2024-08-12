#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#define SPI_SPEED    (1e6)

#define SPI_CHAN_MAIN (0 << 8)
#define SPI_CHAN_AUX  (1 << 8)

#define AIN1_CAL  (12.57/2544)
#define AIN2_CAL  (12.20/2471)
#define AIN3_CAL  (44.95/2221)
#define AIN4_CAL  (44.95/2221)

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


  int h = spiOpen(1, SPI_SPEED, SPI_CHAN_AUX);
  if(h < 0)
  {
    fprintf(stderr, "Error: SPI failed to initialise\n");
    return 1;
  }

  char buf[11];
  int v_1, v_2, v_3, v_4;

  // Burst Read
  buf[0] = (0x01 << 2) | (1 << 0);
  spiXfer(h, buf, buf, 11);

  v_1 = (buf[1] << 8) | buf[2];
  v_2 = (buf[3] << 8) | buf[4];
  v_3 = (buf[5] << 8) | buf[6];
  v_4 = (buf[7] << 8) | buf[8];

  printf("Chan 1: %d\n", v_1);
  printf("Chan 2: %d\n", v_2);
  printf("Chan 3: %d\n", v_3);
  printf("Chan 4: %d\n", v_4);

  float ichrg_voltage = ((float)v_1) * AIN1_CAL;
  float batt_voltage = ((float)v_2) * AIN2_CAL;
  float neg_voltage = ((float)v_3) * AIN3_CAL;
  float pos_voltage = ((float)v_4) * AIN4_CAL;

  printf("Ichrg voltage: %.2fV\n", ichrg_voltage);
  printf("Batt voltage:  %.2fV\n", batt_voltage);

  printf("Vsupply: %.2fV (%.2fV, %.2fV) \n", pos_voltage - neg_voltage, pos_voltage, neg_voltage);

  spiClose(h);

  gpioTerminate();

  return 0;
}
