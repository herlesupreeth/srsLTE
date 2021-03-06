/**
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srsran/phy/resampling/resampler.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/vector.h"
#include <getopt.h>
#include <stdlib.h>
#include <sys/time.h>

static uint32_t buffer_size = 1920;
static uint32_t factor      = 2;
static uint32_t repetitions = 2;

static void usage(char* prog)
{
  printf("Usage: %s [sfr]\n", prog);
  printf("\t-s Buffer size [Default %d]\n", buffer_size);
  printf("\t-f Buffer size [Default %d]\n", factor);
  printf("\t-f r [Default %d]\n", repetitions);
}

static void parse_args(int argc, char** argv)
{
  int opt;

  while ((opt = getopt(argc, argv, "sfr")) != -1) {
    switch (opt) {
      case 's':
        buffer_size = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'f':
        factor = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      case 'r':
        repetitions = (uint32_t)strtol(argv[optind], NULL, 10);
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
}

int main(int argc, char** argv)
{
  struct timeval         t[3]   = {};
  srsran_resampler_fft_t interp = {};
  srsran_resampler_fft_t decim  = {};

  parse_args(argc, argv);

  cf_t* src          = srsran_vec_cf_malloc(buffer_size);
  cf_t* interpolated = srsran_vec_cf_malloc(buffer_size * factor);
  cf_t* decimated    = srsran_vec_cf_malloc(buffer_size);

  if (srsran_resampler_fft_init(&interp, SRSRAN_RESAMPLER_MODE_INTERPOLATE, factor)) {
    return SRSRAN_ERROR;
  }

  if (srsran_resampler_fft_init(&decim, SRSRAN_RESAMPLER_MODE_DECIMATE, factor)) {
    return SRSRAN_ERROR;
  }

  srsran_vec_cf_zero(src, buffer_size);
  srsran_vec_gen_sine(1.0f, 0.01f, src, buffer_size / 10);

  gettimeofday(&t[1], NULL);
  for (uint32_t r = 0; r < repetitions; r++) {
    srsran_resampler_fft_run(&interp, src, interpolated, buffer_size);
    srsran_resampler_fft_run(&decim, interpolated, decimated, buffer_size * factor);
  }
  gettimeofday(&t[2], NULL);
  get_time_interval(t);
  uint64_t duration_us = (uint64_t)(t[0].tv_sec * 1000000UL + t[0].tv_usec);
  printf("Done %.1f Msps\n", factor * buffer_size * repetitions / (double)duration_us);

  //  printf("interp=");
  //  srsran_vec_fprint_c(stdout, interpolated, buffer_size * factor);

  // Check error
  uint32_t delay    = srsran_resampler_fft_get_delay(&decim) * 2;
  uint32_t nsamples = buffer_size - delay;
  srsran_vec_sub_ccc(src, &decimated[delay], interpolated, nsamples);
  float mse = sqrtf(srsran_vec_avg_power_cf(interpolated, nsamples));
  printf("MSE: %f\n", mse);

  //  printf("src=");
  //  srsran_vec_fprint_c(stdout, src, nsamples);
  //  printf("decim=");
  //  srsran_vec_fprint_c(stdout, &decimated[delay], nsamples);

  srsran_resampler_fft_free(&interp);
  srsran_resampler_fft_free(&decim);
  free(src);
  free(interpolated);
  free(decimated);

  return (mse < 0.1f) ? SRSRAN_SUCCESS : SRSRAN_ERROR;
}