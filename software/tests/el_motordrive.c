#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#define WAVEFORM_POINTS     (1000)
#define WAVEFORM_PERIOD_US  (40)

#define AZ_CW_P     (24)
#define AZ_CW_N     (25)

#define AZ_CCW_P    (5) // unused (popped)
#define AZ_CCW_N    (6) // unused (popped)

#define EL_CW_P     (12)
#define EL_CW_N     (13)

#define EL_CCW_P    (16)
#define EL_CCW_N    (26)

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
  printf(" - Max pulses: %d\n",gpioWaveGetMaxPulses());
  printf(" - Max CBs: %d\n",gpioWaveGetMaxCbs());


  float phase_angle, sine_amplitude, duty_cycle;
  int gpio;

  /* Clear Waves */
  int e __attribute__((unused));
  e = gpioWaveClear();

  gpioPulse_t *dir_a_wf = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(dir_a_wf == NULL)
  {
    fprintf(stderr, "Error allocating local memory for waveform\n");
    return 1;
  }

  for(int i = 0; i < WAVEFORM_POINTS; i++)
  {
    phase_angle = i * ((2*M_PI) / (float)WAVEFORM_POINTS);
    sine_amplitude = sin(phase_angle);

    if(sine_amplitude >= 0)
    {
      /* Positive half of wave */
      gpio = 1 << (EL_CW_P);
    }
    else
    {
      gpio = 1 << (EL_CW_N);
    }

    duty_cycle = fabs(sine_amplitude) * 0.9;

    if((i & 0x01) == 0x00)
    {
      dir_a_wf[i].gpioOn = gpio;
      dir_a_wf[i].gpioOff = 0;
      dir_a_wf[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      printf("%06d (%f dc)\n", dir_a_wf[i].usDelay, duty_cycle);
    }
    else
    {
      dir_a_wf[i].gpioOn = 0;
      dir_a_wf[i].gpioOff = gpio;
      dir_a_wf[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
    }
  }

  int dir_a_points = gpioWaveAddGeneric(WAVEFORM_POINTS, dir_a_wf);
  printf("Dir A Pulses: %d\n", dir_a_points);

  int dir_a_wave __attribute__((unused)) = gpioWaveCreate();

  /* Set up outputs */

  gpioSetMode(AZ_CW_P, PI_OUTPUT);
  gpioSetMode(AZ_CW_N, PI_OUTPUT);

  gpioSetMode(AZ_CCW_P, PI_OUTPUT);
  gpioSetMode(AZ_CCW_N, PI_OUTPUT);

  gpioSetMode(EL_CW_P, PI_OUTPUT);
  gpioSetMode(EL_CW_N, PI_OUTPUT);

  gpioSetMode(EL_CCW_P, PI_OUTPUT);
  gpioSetMode(EL_CCW_N, PI_OUTPUT);

  /* Direction Relays */
  gpioSetMode(AZ_SWITCH_GPIO, PI_OUTPUT);
  gpioSetMode(EL_SWITCH_GPIO, PI_OUTPUT);

  gpioWrite(AZ_SWITCH_GPIO, 0);
  gpioWrite(EL_SWITCH_GPIO, 0);

  /* Run waves */
  while(!app_exit)
  {
    /*
    gpioWaveChain((char []) {
      255, 0,                       // loop start
         dir_a_wave,               // transmit waves 0+1
      255, 1, 200, 0,                 // loop end (repeat 200 times)
      255, 0,                       // loop start
         dir_b_wave,               // transmit waves 0+1
      255, 1, 200, 0,                 // loop end (repeat 200 times)
      }, 14);

   while (gpioWaveTxBusy()) time_sleep(0.1);
    printf("A\n");
    */

    gpioWrite(EL_SWITCH_GPIO, 1);


    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

    e = gpioWaveTxSend(dir_a_wave, PI_WAVE_MODE_REPEAT);
    printf("A\n");
    sleep(5);
    e = gpioWaveTxStop();
    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

    gpioWrite(EL_SWITCH_GPIO, 1);

    if(app_exit) break;

    //sleep(1);

    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

    e = gpioWaveTxSend(dir_a_wave, PI_WAVE_MODE_REPEAT);
    printf("A\n");
    sleep(5);
    e = gpioWaveTxStop();
    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

    gpioWrite(EL_SWITCH_GPIO, 1);

    if(app_exit) break;

    //sleep(1);

/*

    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

    e = gpioWaveTxSend(dir_b_wave, PI_WAVE_MODE_REPEAT);
    printf("B\n");
    sleep(5);
    e = gpioWaveTxStop();
    gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );
*/
  }

  printf("Closing down GPIO..\n");

  e = gpioWaveTxStop();

  gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (EL_CW_P) | 1 << (EL_CW_N)
      | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    );

  gpioWrite(AZ_SWITCH_GPIO, 0);
  gpioWrite(EL_SWITCH_GPIO, 0);

  gpioTerminate();

  return 0;
}
