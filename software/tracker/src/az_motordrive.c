#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"

#define WAVEFORM_POINTS     (1000)
#define WAVEFORM_PERIOD_US  (40)

#define AZ_CW_P     (24)
#define AZ_CW_N     (25)

#define AZ_CCW_P    (5) // unused (popped)
#define AZ_CCW_N    (6) // unused (popped)

#define AZ_SWITCH_GPIO  (27)

void *az_motordrive_thread(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;


  float phase_angle, sine_amplitude, duty_cycle;
  int gpio;

  /* Clear Waves */
  int e __attribute__((unused));

  gpioPulse_t *dir_a_wf = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(dir_a_wf == NULL)
  {
    fprintf(stderr, "Error allocating local memory for waveform\n");
    return NULL;
  }

  for(int i = 0; i < WAVEFORM_POINTS; i++)
  {
    phase_angle = i * ((2*M_PI) / (float)WAVEFORM_POINTS);
    sine_amplitude = sin(phase_angle);

    if(sine_amplitude >= 0)
    {
      /* Positive half of wave */
      gpio = 1 << (AZ_CW_P);
    }
    else
    {
      gpio = 1 << (AZ_CW_N);
    }

    duty_cycle = fabs(sine_amplitude) * 0.9;

    if((i & 0x01) == 0x00)
    {
      dir_a_wf[i].gpioOn = gpio;
      dir_a_wf[i].gpioOff = 0;
      dir_a_wf[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      //printf("%06d (%f dc)\n", dir_a_wf[i].usDelay, duty_cycle);
    }
    else
    {
      dir_a_wf[i].gpioOn = 0;
      dir_a_wf[i].gpioOff = gpio;
      dir_a_wf[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
    }
  }

  e = gpioWaveAddGeneric(WAVEFORM_POINTS, dir_a_wf);
  if(e < 0)
  {
    fprintf(stderr, "Failed to add Az Waveform to gpio: %d\n", e);
  }
  //int dir_a_points = gpioWaveAddGeneric(WAVEFORM_POINTS, dir_a_wf);
  //printf("Dir A Pulses: %d\n", dir_a_points);

  int dir_a_wave __attribute__((unused)) = gpioWaveCreate();
  if(dir_a_wave < 0)
  {
    fprintf(stderr, "Failed to create Az Waveform: %d\n", e);
  }

  /* Set up outputs */

  gpioSetMode(AZ_CW_P, PI_OUTPUT);
  gpioSetMode(AZ_CW_N, PI_OUTPUT);

  gpioSetMode(AZ_CCW_P, PI_OUTPUT);
  gpioSetMode(AZ_CCW_N, PI_OUTPUT);

  gpioWrite_Bits_0_31_Clear(
    1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
  );

  /* Direction Relays */
  gpioSetMode(AZ_SWITCH_GPIO, PI_OUTPUT);

  gpioWrite(AZ_SWITCH_GPIO, 0);

  int32_t relay_state = 0;
  int32_t pwm_state = 0;

  /* Run waves */
  while(!(app_state->app_exit))
  {
    if(app_state->demand_valid
      && app_state->current_valid
    )
    {
      if(app_state->demand_az_deg < (app_state->current_az_deg - 1.5)) // 0.5 hysteresis
      {
        if(relay_state == 0)
        {
          gpioWrite(AZ_SWITCH_GPIO, 1);
          relay_state = 1;

          usleep(300*1000);
        }
        if(pwm_state == 0)
        {
          e = gpioWaveTxSend(dir_a_wave, PI_WAVE_MODE_REPEAT);
          pwm_state = 1;
        }
      }
      else if(app_state->demand_az_deg > (app_state->current_az_deg + 1.5)) // 0.5 hysteresis
      {
        if(relay_state == 1)
        {
          gpioWrite(AZ_SWITCH_GPIO, 0);
          relay_state = 0;

          usleep(300*1000);
        }

        if(pwm_state == 0)
        {
          printf("driving cw..");
          e = gpioWaveTxSend(dir_a_wave, PI_WAVE_MODE_REPEAT);
          if(e < 0)
          {
            fprintf(stderr, "Failed to send Az drive waveform: %d\n", e);
          }
          pwm_state = 1;
        }
      }
      else
      {
        /* Can stop */

        if(pwm_state == 1)
        {
          gpioWaveTxStop();
          gpioWrite_Bits_0_31_Clear(
            1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
          );

          pwm_state = 0;
        }
      }
    }
    else
    {
      /* Not valid */

      if(pwm_state == 1)
      {
        gpioWaveTxStop();
        gpioWrite_Bits_0_31_Clear(
          1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
        );

        pwm_state = 0;
      }
    }

    usleep(100*1000);
  }

  printf("Az motordrive thread exiting..\n");

  gpioWaveTxStop();

  gpioWrite_Bits_0_31_Clear(
      1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
    );

  gpioWrite(AZ_SWITCH_GPIO, 0);

  return NULL;
}
