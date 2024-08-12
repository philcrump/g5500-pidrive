#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"
#include "util/timing.h"


#define AZ_HYSTERESIS   (1.5)
#define EL_HYSTERESIS   (1.5)

#define AZ_RELAY_PAUSE_MS   (300)
#define EL_RELAY_PAUSE_MS   (300)

#define WAVEFORM_POINTS     (1000)
#define WAVEFORM_PERIOD_US  (40)

#define AZ_CW_P     (24)
#define AZ_CW_N     (25)

#define AZ_CCW_P    (5) // unused (popped)
#define AZ_CCW_N    (6) // unused (popped)

#define EL_CW_P     (12)
#define EL_CW_N     (13)

#define EL_CCW_P    (16) //unused
#define EL_CCW_N    (26) // unused

#define AZ_SWITCH_GPIO  (27)
#define EL_SWITCH_GPIO  (22)

void *motordrive_thread(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  float phase_angle, sine_amplitude, duty_cycle;

  gpioPulse_t *az_wf = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(az_wf == NULL)
  {
    fprintf(stderr, "Motordrive: Error allocating local memory for waveform\n");
    return NULL;
  }

  gpioPulse_t *el_wf = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(el_wf == NULL)
  {
    fprintf(stderr, "Motordrive: Error allocating local memory for waveform\n");
    free(az_wf);
    return NULL;
  }

  gpioPulse_t *azel_wf = (gpioPulse_t *)malloc(WAVEFORM_POINTS * sizeof(gpioPulse_t));
  if(azel_wf == NULL)
  {
    fprintf(stderr, "Motordrive: Error allocating local memory for waveform\n");
    free(az_wf);
    free(el_wf);
    return NULL;
  }

  int gpio_az, gpio_el;

  for(int i = 0; i < WAVEFORM_POINTS; i++)
  {
    phase_angle = i * ((2*M_PI) / (float)WAVEFORM_POINTS);
    sine_amplitude = sin(phase_angle);

    if(sine_amplitude >= 0)
    {
      /* Positive half of wave */
      gpio_az = 1 << (AZ_CW_P);
      gpio_el = 1 << (EL_CW_P);
    }
    else
    {
      gpio_az = 1 << (AZ_CW_N);
      gpio_el = 1 << (EL_CW_N);
    }

    duty_cycle = fabs(sine_amplitude) * 0.9;

    if((i & 0x01) == 0x00)
    {
      az_wf[i].gpioOn = gpio_az;
      el_wf[i].gpioOn = gpio_el;
      azel_wf[i].gpioOn = (gpio_az | gpio_el);

      az_wf[i].gpioOff = 0;
      el_wf[i].gpioOff = 0;
      azel_wf[i].gpioOff = 0;

      az_wf[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      el_wf[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      azel_wf[i].usDelay = (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
    }
    else
    {

      az_wf[i].gpioOn = 0;
      el_wf[i].gpioOn = 0;
      azel_wf[i].gpioOn = 0;

      az_wf[i].gpioOff = gpio_az;
      el_wf[i].gpioOff = gpio_el;
      azel_wf[i].gpioOff = (gpio_az | gpio_el);

      az_wf[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      el_wf[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
      azel_wf[i].usDelay = WAVEFORM_PERIOD_US - (int)(round(duty_cycle * WAVEFORM_PERIOD_US));
    }
  }

  int e, az_wave, el_wave, azel_wave;

  e = gpioWaveAddGeneric(WAVEFORM_POINTS, az_wf);
  if(e < 0)
  {
    fprintf(stderr, "Failed to add az_wf Waveform to gpio: %d\n", e);
  }
  az_wave = gpioWaveCreate();
  if(az_wave < 0)
  {
    fprintf(stderr, "Failed to create az_wave Waveform: %d\n", e);
  }

  e = gpioWaveAddGeneric(WAVEFORM_POINTS, el_wf);
  if(e < 0)
  {
    fprintf(stderr, "Failed to add el_wf Waveform to gpio: %d\n", e);
  }
  el_wave = gpioWaveCreate();
  if(el_wave < 0)
  {
    fprintf(stderr, "Failed to create el_wave Waveform: %d\n", e);
  }

  e = gpioWaveAddGeneric(WAVEFORM_POINTS, azel_wf);
  if(e < 0)
  {
    fprintf(stderr, "Failed to add azel_wf Waveform to gpio: %d\n", e);
  }
  azel_wave = gpioWaveCreate();
  if(azel_wave < 0)
  {
    fprintf(stderr, "Failed to create azel_wave Waveform: %d\n", e);
  }

  /* Set up outputs */

  gpioSetMode(AZ_CW_P, PI_OUTPUT);
  gpioSetMode(AZ_CW_N, PI_OUTPUT);

  gpioSetMode(AZ_CCW_P, PI_OUTPUT);
  gpioSetMode(AZ_CCW_N, PI_OUTPUT);

  gpioSetMode(EL_CW_P, PI_OUTPUT);
  gpioSetMode(EL_CW_N, PI_OUTPUT);

  gpioSetMode(EL_CCW_P, PI_OUTPUT);
  gpioSetMode(EL_CCW_N, PI_OUTPUT);

  gpioWrite_Bits_0_31_Clear(
    1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
    | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
  );

  /* Direction Relays */
  gpioSetMode(AZ_SWITCH_GPIO, PI_OUTPUT);
  gpioSetMode(EL_SWITCH_GPIO, PI_OUTPUT);

  gpioWrite(AZ_SWITCH_GPIO, 0);
  gpioWrite(EL_SWITCH_GPIO, 0);

  enum WAVE_STATE {
    WAVE_OFF,
    WAVE_AZ,
    WAVE_EL,
    WAVE_AZEL
  };

  int32_t pwm_state = WAVE_OFF;

  int32_t az_relay_state = 0;
  int32_t el_relay_state = 0;

  uint64_t az_relay_lastswitch = timestamp_ms();
  uint64_t el_relay_lastswitch = timestamp_ms();

  /* Run waves */
  while(!(app_state->app_exit))
  {
    if(app_state->demand_valid
      && app_state->current_valid
    )
    {
      // Check Azimuth
      if(app_state->demand_az_deg < (app_state->current_az_deg - AZ_HYSTERESIS)
        && (az_relay_lastswitch + AZ_RELAY_PAUSE_MS) < timestamp_ms())
      {
        if(az_relay_state == 0)
        {
          gpioWrite(AZ_SWITCH_GPIO, 1);
          az_relay_state = 1;

          az_relay_lastswitch = timestamp_ms();
        }
        else
        {
          if(pwm_state != WAVE_AZ && pwm_state != WAVE_AZEL)
          {
            if(pwm_state == WAVE_EL)
            {
              /* Stop the El-only wave */
              gpioWaveTxStop();
              gpioWrite_Bits_0_31_Clear(
                1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
                | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
              );
              /* Start the az-el wave */
              e = gpioWaveTxSend(azel_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send Az drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZEL;
            }
            else
            {
              /* Start the az-only wave */
              e = gpioWaveTxSend(az_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send Az drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZ;
            }
          }
        }
      }
      else if(app_state->demand_az_deg > (app_state->current_az_deg + AZ_HYSTERESIS)
        && (az_relay_lastswitch + AZ_RELAY_PAUSE_MS) < timestamp_ms())
      {
        if(az_relay_state == 1)
        {
          gpioWrite(AZ_SWITCH_GPIO, 0);
          az_relay_state = 0;

          az_relay_lastswitch = timestamp_ms();
        }
        else
        {
          if(pwm_state != WAVE_AZ && pwm_state != WAVE_AZEL)
          {
            if(pwm_state == WAVE_EL)
            {
              /* Stop the El-only wave */
              gpioWaveTxStop();
              gpioWrite_Bits_0_31_Clear(
                1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
                | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
              );
              /* Start the az-el wave */
              e = gpioWaveTxSend(azel_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send Az drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZEL;
            }
            else
            {
              /* Start the az-only wave */
              e = gpioWaveTxSend(az_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send Az drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZ;
            }
          }
        }
      }
      else
      {
        /* Can stop */

        if(pwm_state == WAVE_AZ || pwm_state == WAVE_AZEL)
        {
          gpioWaveTxStop();
          gpioWrite_Bits_0_31_Clear(
            1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
            | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
          );

          if(pwm_state == WAVE_AZEL)
          {
            /* Start the el-only wave */
            e = gpioWaveTxSend(el_wave, PI_WAVE_MODE_REPEAT);
            if(e < 0)
            {
              fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
            }
            pwm_state = WAVE_EL;
          }
          else
          {
            pwm_state = WAVE_OFF;
          }
        }
      }

      // Check Elevation
      if(app_state->demand_el_deg < (app_state->current_el_deg - EL_HYSTERESIS)
        && (el_relay_lastswitch + EL_RELAY_PAUSE_MS) < timestamp_ms())
      {
        if(el_relay_state == 1)
        {
          gpioWrite(EL_SWITCH_GPIO, 0);
          el_relay_state = 0;

          el_relay_lastswitch = timestamp_ms();
        }
        else
        {
          if(pwm_state != WAVE_EL && pwm_state != WAVE_AZEL)
          {
            if(pwm_state == WAVE_AZ)
            {
              /* Stop the Az-only wave */
              gpioWaveTxStop();
              gpioWrite_Bits_0_31_Clear(
                1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
                | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
              );
              /* Start the az-el wave */
              e = gpioWaveTxSend(azel_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZEL;
            }
            else
            {
              /* Start the el-only wave */
              e = gpioWaveTxSend(el_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
              }
              pwm_state = WAVE_EL;
            }
          }
        }
      }
      else if(app_state->demand_el_deg > (app_state->current_el_deg + EL_HYSTERESIS)
        && (el_relay_lastswitch + EL_RELAY_PAUSE_MS) < timestamp_ms())
      {
        if(el_relay_state == 0)
        {
          gpioWrite(EL_SWITCH_GPIO, 1);
          el_relay_state = 1;

          el_relay_lastswitch = timestamp_ms();
        }
        else
        {
          if(pwm_state != WAVE_EL && pwm_state != WAVE_AZEL)
          {
            if(pwm_state == WAVE_AZ)
            {
              /* Stop the Az-only wave */
              gpioWaveTxStop();
              gpioWrite_Bits_0_31_Clear(
                1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
                | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
              );
              /* Start the az-el wave */
              e = gpioWaveTxSend(azel_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
              }
              pwm_state = WAVE_AZEL;
            }
            else
            {
              /* Start the el-only wave */
              e = gpioWaveTxSend(el_wave, PI_WAVE_MODE_REPEAT);
              if(e < 0)
              {
                fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
              }
              pwm_state = WAVE_EL;
            }
          }
        }
      }
      else
      {
        /* Can stop */

        if(pwm_state == WAVE_EL || pwm_state == WAVE_AZEL)
        {
          gpioWaveTxStop();
          gpioWrite_Bits_0_31_Clear(
            1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
            | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
          );

          if(pwm_state == WAVE_AZEL)
          {
            /* Start the az-only wave */
            e = gpioWaveTxSend(az_wave, PI_WAVE_MODE_REPEAT);
            if(e < 0)
            {
              fprintf(stderr, "Failed to send El drive waveform: %d\n", e);
            }
            pwm_state = WAVE_AZ;
          }
          else
          {
            pwm_state = WAVE_OFF;
          }
        }
      }
    }
    else
    {
      /* Not valid */

      if(pwm_state != WAVE_OFF)
      {
        gpioWaveTxStop();
        gpioWrite_Bits_0_31_Clear(
          1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
          | 1 << (AZ_CW_P) | 1 << (AZ_CW_N) | 1 << (AZ_CCW_P) | 1 << (AZ_CCW_N)
        );

        pwm_state = WAVE_OFF;
      }
    }

    usleep(100*1000);
  }

  printf("Motordrive thread exiting..\n");

  gpioWaveTxStop();

  gpioWrite_Bits_0_31_Clear(
    1 << (EL_CW_P) | 1 << (EL_CW_N) | 1 << (EL_CCW_P) | 1 << (EL_CCW_N)
  );

  gpioWrite(EL_SWITCH_GPIO, 0);

  return NULL;
}
