#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include <math.h>
#include <pigpio.h>

#include "main.h"

#define SPI_SPEED    (1e6)

#define SPI_CHAN_MAIN (0 << 8)
#define SPI_CHAN_AUX  (1 << 8)

#define ADC_CHAN_AZ   (0 << 6)
#define ADC_CHAN_EL   (1 << 6)

#define EL_ADC_0    (3703)
#define EL_ADC_90   (2015)

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

void *adc_loop(void *arg)
{
  app_state_t *app_state = (app_state_t *)arg;

  int h;
  char buf[11];

  int32_t el_adc, az_adc;

  int v_1, v_2, v_3, v_4;
  float ichrg_voltage, batt_voltage, neg_voltage, pos_voltage;

  // Set incorrectly-wired Supplies ADC CS pin as high-z
  gpioSetMode(7, PI_INPUT);

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

    app_state->current_az_deg = azimuth_deg_from_adc(az_adc);

    app_state->current_el_deg = elevation_deg_from_adc(el_adc);

    app_state->current_valid = true;


    h = spiOpen(1, SPI_SPEED, SPI_CHAN_AUX);

    // Burst Read
    buf[0] = (0x01 << 2) | (1 << 0);
    spiXfer(h, buf, buf, 11);

    spiClose(h);

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

    app_state->poe_voltage = ichrg_voltage;
    app_state->batt_voltage = batt_voltage;
    app_state->high_voltage = (pos_voltage - neg_voltage);
    app_state->voltages_valid = true;
    

    usleep(200*1000); // 5 Hz
  }

  printf("ADC Encoder thread exiting..\n");

  return NULL;
}
