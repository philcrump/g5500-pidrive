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

#define ADC_CHAN_AZ   (1 << 6)
#define ADC_CHAN_EL   (0 << 6)

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


  int h = spiOpen(0, SPI_SPEED, SPI_CHAN_MAIN);
  if(h < 0)
  {
    fprintf(stderr, "Error: SPI failed to initialise\n");
    return 1;
  }

  char buf[3];
  int v;

  buf[0] = 1;
  buf[1] = (1 << 7) | ADC_CHAN_AZ | (1 << 5);
  buf[2] = 0;
  spiXfer(h, buf, buf, 3);

  v = ((buf[1]&0xf)<<8) | buf[2];

  printf("Azimuth: %.2fV (%d)\n", ((float)v) * (1.8/(1<<12)), v);

  buf[0] = 1;
  buf[1] = (1 << 7) | ADC_CHAN_EL | (1 << 5);
  buf[2] = 0;
  spiXfer(h, buf, buf, 3);

  v = ((buf[1]&0xf)<<8) | buf[2];

  printf("Elevation: %.2fV (%d)\n", ((float)v) * (1.8/(1<<12)), v);

  spiClose(h);

  gpioTerminate();

  return 0;
}
