/*
 * Copyright (c) 2002, 2017 Jens Keiner, Stefan Kunis, Daniel Potts
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>

#include <sys/time.h>

#define NFFT_PRECISION_DOUBLE

#include "nfft3mp.h"

int main(void)
{
  NFFT(plan) p;
  const int N = 1000000;
  const int M = 1000000;
  NFFT_R t0, t1;

  printf("nthreads = " NFFT__D__ "\n", NFFT(get_num_threads)());

  /* init */
  FFTW(init_threads)();
  NFFT(init_1d)(&p,N,M);

  /* pseudo random nodes */
  NFFT(vrand_shifted_unit_double)(p.x,p.M_total);

  /* precompute psi, that is, the entries of the matrix B */
  t0 = NFFT(clock_gettime_seconds)();
  if(p.flags & PRE_ONE_PSI)
      NFFT(precompute_one_psi)(&p);
  t1 = NFFT(clock_gettime_seconds)();
  fprintf(stderr,"precompute elapsed time: %.3" NFFT__FIS__ " seconds\n",t1-t0);

  /* pseudo random Fourier coefficients */
  NFFT(vrand_unit_complex)(p.f_hat,p.N_total);

  /* transformation */
  t0 = NFFT(clock_gettime_seconds)();
  NFFT(trafo)(&p);
  t1 = NFFT(clock_gettime_seconds)();
  fprintf(stderr,"compute    elapsed time: %.3" NFFT__FIS__ " seconds\n",t1-t0);
  fflush(stderr);

  /* cleanup */
  NFFT(finalize)(&p);
  FFTW(cleanup_threads)();

  return EXIT_SUCCESS;
}
