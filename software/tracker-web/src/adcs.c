#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"
#include "util/timing.h"

#define SPI_SPEED    (1e6)

#define SPI_CHAN_MAIN (0 << 8)
#define SPI_CHAN_AUX  (1 << 8)

#define ADC_CHAN_AZ   (0 << 6)
#define ADC_CHAN_EL   (1 << 6)

// Elevation: 0: 3625
// Elevation: 90: 1882
// Elevation: 180: 196

#define EL_ADC_0    (3625)
#define EL_ADC_90   (1882)


// endstops:  -45.7: 199, 401.0: 3574


// 360: 3265
// 180: 1916
// 0: 544

#define AZ_ADC_0    (544)
#define AZ_ADC_360  (3264)

#define AIN1_CAL  (12.57/2544)
#define AIN2_CAL  (12.20/2471)
#define AIN3_CAL  (44.95/2221)
#define AIN4_CAL  (44.95/2221)

static double elevation_deg_from_adc(int32_t adc)
{
  return 90.0 - (((double)(adc - EL_ADC_90) / (EL_ADC_0 - EL_ADC_90)) * 90.0);
}

static double azimuth_deg_from_adc(int32_t adc)
{
  return ((double)(adc - AZ_ADC_0) / (AZ_ADC_360 - AZ_ADC_0)) * 360.0;
}

void *loop_adcs(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  int h, h2;
  char buf[11];

  int32_t el_adc, az_adc;

  int v_1, v_2, v_3, v_4;
  float ichrg_voltage, batt_voltage, neg_voltage, pos_voltage;

  while(!(app_state->app_exit))
  {
    h = spiOpen(0, SPI_SPEED, SPI_CHAN_MAIN);

    buf[0] = 1;
    buf[1] = (1 << 7) | ADC_CHAN_AZ | (1 << 5);
    buf[2] = 0;
    spiXfer(h, buf, buf, 3);

    az_adc = ((buf[1]&0xf)<<8) | buf[2];

    buf[0] = 1;
    buf[1] = (1 << 7) | ADC_CHAN_EL | (1 << 5);
    buf[2] = 0;
    spiXfer(h, buf, buf, 3);

    el_adc = ((buf[1]&0xf)<<8) | buf[2];

    spiClose(h);

    //printf("az adc: %d\n", az_adc);
    //printf("el adc: %d\n", el_adc);

    if(app_state->headingoffset_valid)
    {
      app_state->encoder_az_deg = azimuth_deg_from_adc(az_adc) + app_state->headingoffset_deg;
    }
    else
    {
      app_state->encoder_az_deg = azimuth_deg_from_adc(az_adc);
    }
    app_state->encoder_el_deg = elevation_deg_from_adc(el_adc);

    app_state->encoder_valid = true;
    app_state->encoder_lastupdated_ms = timestamp_ms();


    // Set incorrectly-wired Voltages ADC CS pin as high-z
    gpioSetMode(7, PI_INPUT);

    h2 = spiOpen(1, SPI_SPEED, SPI_CHAN_AUX);

    // Burst Read
    buf[0] = (0x01 << 2) | (1 << 0);
    spiXfer(h2, buf, buf, 11);

    spiClose(h2);

    v_1 = (buf[1] << 8) | buf[2];
    v_2 = (buf[3] << 8) | buf[4];
    v_3 = (buf[5] << 8) | buf[6];
    v_4 = (buf[7] << 8) | buf[8];

    //printf("Chan 1: %d\n", v_1);
    //printf("Chan 2: %d\n", v_2);
    //printf("Chan 3: %d\n", v_3);
    //printf("Chan 4: %d\n", v_4);

    ichrg_voltage = ((float)v_1) * AIN1_CAL;
    batt_voltage = ((float)v_2) * AIN2_CAL;
    neg_voltage = ((float)v_3) * AIN3_CAL;
    pos_voltage = ((float)v_4) * AIN4_CAL;

    //printf("Ichrg voltage: %.2fV\n", ichrg_voltage);
    //printf("Batt voltage:  %.2fV\n", batt_voltage);

    //printf("Vsupply: %.2fV (%.2fV, %.2fV) \n", pos_voltage - neg_voltage, pos_voltage, neg_voltage);

    app_state->voltages_poe = ichrg_voltage;
    app_state->voltages_ichrg = (ichrg_voltage - batt_voltage) / 0.7;
    app_state->voltages_batt = batt_voltage;
    app_state->voltages_high = (pos_voltage - neg_voltage);

    app_state->voltages_valid = true;
    app_state->voltages_lastupdated_ms = timestamp_ms();

    sleep_ms(100);
  }

  printf("ADC Encoder thread exiting..\n");

  pthread_exit(NULL);
}
