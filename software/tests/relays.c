#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#define AZ_SWITCH_GPIO  (27)
#define EL_SWITCH_GPIO  (22)


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

  gpioSetMode(AZ_SWITCH_GPIO, PI_OUTPUT);
  gpioSetMode(EL_SWITCH_GPIO, PI_OUTPUT);

  gpioWrite(AZ_SWITCH_GPIO, 0);
  gpioWrite(EL_SWITCH_GPIO, 0);

  gpioWrite(AZ_SWITCH_GPIO, 1);

  sleep(1);

  gpioWrite(EL_SWITCH_GPIO, 1);

  sleep(1);

  gpioWrite(AZ_SWITCH_GPIO, 0);
  gpioWrite(EL_SWITCH_GPIO, 0);

  printf("Closing down GPIO..\n");

  gpioTerminate();

  return 0;
}
