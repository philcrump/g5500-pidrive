#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#define WAVEFORM_POINTS 1000
#define WAVEFORM_PERIOD_US 40 //40

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

  int status;

  status = gpioInitialise();

  printf("pigpio version %d.\n", gpioVersion());
  printf("Hardware revision %d.\n", gpioHardwareRevision());
  printf(" - Max pulses: %d\n",gpioWaveGetMaxPulses());
  printf(" - Max CBs: %d\n",gpioWaveGetMaxCbs());

  gpioPulse_t *wf_ptr = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(wf_ptr == NULL)
  {
    fprintf(stderr, "Error allocating local memory for waveform\n");
    return 1;
  }

  float phase_angle, sine_amplitude, duty_cycle;
  int gpio;
  for(int i = 0; i < WAVEFORM_POINTS; i++)
  {
    phase_angle = i * ((2*M_PI) / (float)WAVEFORM_POINTS);
    sine_amplitude = sin(phase_angle);

    if(sine_amplitude >= 0)
    {
      /* First half of wave */
      gpio = 1<<(22);
    }
    else
    {
      gpio = 1<<(27);
    }

    duty_cycle = fabs(sine_amplitude);

    if((i & 0x01) == 0x00)
    {
      wf_ptr[i].gpioOn = gpio;
      wf_ptr[i].gpioOff = 0;
      wf_ptr[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      printf("%06d (%f dc)\n", wf_ptr[i].usDelay, duty_cycle);
    }
    else
    {
      wf_ptr[i].gpioOn = 0;
      wf_ptr[i].gpioOff = gpio;
      wf_ptr[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
    }
  }


  int e, wid;

  gpioSetMode(22, PI_OUTPUT);
  gpioSetMode(27, PI_OUTPUT);
  e = gpioWaveClear();

  e = gpioWaveAddGeneric(WAVEFORM_POINTS, wf_ptr);

  printf("Pulses: %d\n", e);

  wid = gpioWaveCreate();
  e = gpioWaveTxSend(wid, PI_WAVE_MODE_REPEAT);

  while(!app_exit)
  {
  }

  e = gpioWaveTxStop();

  gpioTerminate();

  return 0;
}
