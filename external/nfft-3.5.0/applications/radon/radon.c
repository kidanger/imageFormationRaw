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

/**
 * \file radon.c
 * \brief NFFT-based discrete Radon transform.
 *
 * Computes the discrete Radon transform
 * \f[
 *    R_{\theta_t} f\left(\frac{s}{R}\right)
 *    = \sum_{r \in I_R} w_r \; \sum_{k \in I_N^2} f_{k}
 *        \mathrm{e}^{-2\pi\mathrm{I} k \; (\frac{r}{R}\theta_t)}
 *        \, \mathrm{e}^{2\pi\mathrm{i} r s / R}
 *    \qquad(t \in I_T, s \in I_R).
 * \f]
 * by taking the 2D-NFFT of \f$f_k\f$ (\f$k \in I_N^2\f$)
 * at the points \f$\frac{r}{R}\theta_t\f$ of the polar or linogram grid
 * followed by 1D-iFFTs for every direction \f$t \in T\f$,
 * where \f$w_r\f$ are the weights of the Dirichlet- or Fejer-kernel.
 * \author Markus Fenn
 * \date 2005
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>

#define NFFT_PRECISION_DOUBLE

#include "nfft3mp.h"

/** define weights of kernel function for discrete Radon transform */
/*#define KERNEL(r) 1.0 */
#define KERNEL(r) (NFFT_K(1.0)-NFFT_M(fabs)((NFFT_R)(r))/((NFFT_R)S/2))

/** generates the points x with weights w
 *  for the polar grid with T angles and R offsets
 */
static int polar_grid(int T, int S, NFFT_R *x, NFFT_R *w)
{
  int t, r;
  NFFT_R W = (NFFT_R) T * (((NFFT_R) S / NFFT_K(2.0)) * ((NFFT_R) S / NFFT_K(2.0)) + NFFT_K(1.0) / NFFT_K(4.0));

  for (t = -T / 2; t < T / 2; t++)
  {
    for (r = -S / 2; r < S / 2; r++)
    {
      x[2 * ((t + T / 2) * S + (r + S / 2)) + 0] = (NFFT_R) r / (NFFT_R)(S) * NFFT_M(cos)(NFFT_KPI * (NFFT_R)(t) / (NFFT_R)(T));
      x[2 * ((t + T / 2) * S + (r + S / 2)) + 1] = (NFFT_R) r / (NFFT_R)(S) * NFFT_M(sin)(NFFT_KPI * (NFFT_R)(t) / (NFFT_R)(T));
      if (r == 0)
        w[(t + T / 2) * S + (r + S / 2)] = NFFT_K(1.0) / NFFT_K(4.0) / W;
      else
        w[(t + T / 2) * S + (r + S / 2)] = NFFT_M(fabs)((NFFT_R) r) / W;
    }
  }

  return 0;
}

/** generates the points x with weights w
 *  for the linogram grid with T slopes and R offsets
 */
static int linogram_grid(int T, int S, NFFT_R *x, NFFT_R *w)
{
  int t, r;
  NFFT_R W = (NFFT_R) T * (((NFFT_R) S / NFFT_K(2.0)) * ((NFFT_R) S / NFFT_K(2.0)) + NFFT_K(1.0) / NFFT_K(4.0));

  for (t = -T / 2; t < T / 2; t++)
  {
    for (r = -S / 2; r < S / 2; r++)
    {
      if (t < 0)
      {
        x[2 * ((t + T / 2) * S + (r + S / 2)) + 0] = (NFFT_R) r / (NFFT_R)(S);
        x[2 * ((t + T / 2) * S + (r + S / 2)) + 1] = NFFT_K(4.0) * ((NFFT_R)(t) + (NFFT_R)(T) / NFFT_K(4.0)) / (NFFT_R)(T) * (NFFT_R)(r)
            / (NFFT_R)(S);
      }
      else
      {
        x[2 * ((t + T / 2) * S + (r + S / 2)) + 0] = -NFFT_K(4.0) * ((NFFT_R)(t) - (NFFT_R)(T) / NFFT_K(4.0)) / (NFFT_R)(T)
            * (NFFT_R)(r) / (NFFT_R)(S);
        x[2 * ((t + T / 2) * S + (r + S / 2)) + 1] = (NFFT_R) r / (NFFT_R)(S);
      }
      if (r == 0)
        w[(t + T / 2) * S + (r + S / 2)] = NFFT_K(1.0) / NFFT_K(4.0) / W;
      else
        w[(t + T / 2) * S + (r + S / 2)] = NFFT_M(fabs)((NFFT_R) r) / W;
    }
  }

  return 0;
}

/** computes the NFFT-based discrete Radon transform of f
 *  on the grid given by gridfcn() with T angles and R offsets
 */
static int Radon_trafo(int (*gridfcn)(), int T, int S, NFFT_R *f, int NN, NFFT_R *Rf)
{
  int j, k; /**< index for nodes and freqencies   */
  NFFT(plan) my_nfft_plan; /**< plan for the nfft-2D             */

  NFFT_C *fft; /**< variable for the fftw-1Ds        */
  FFTW(plan) my_fftw_plan; /**< plan for the fftw-1Ds            */

  int t, r; /**< index for directions and offsets */
  NFFT_R *x, *w; /**< knots and associated weights     */

  int N[2], n[2];
  int M = T * S;

  N[0] = NN;
  n[0] = 2 * N[0];
  N[1] = NN;
  n[1] = 2 * N[1];

  fft = (NFFT_C *) NFFT(malloc)((size_t)(S) * sizeof(NFFT_C));
  my_fftw_plan = FFTW(plan_dft_1d)(S, fft, fft, FFTW_BACKWARD, FFTW_MEASURE);

  x = (NFFT_R *) NFFT(malloc)((size_t)(2 * T * S) * (sizeof(NFFT_R)));
  if (x == NULL)
    return EXIT_FAILURE;

  w = (NFFT_R *) NFFT(malloc)((size_t)(T * S) * (sizeof(NFFT_R)));
  if (w == NULL)
    return EXIT_FAILURE;

  /** init two dimensional NFFT plan */
  NFFT(init_guru)(&my_nfft_plan, 2, N, M, n, 4,
      PRE_PHI_HUT | PRE_PSI | MALLOC_X | MALLOC_F_HAT | MALLOC_F | FFTW_INIT
          | FFT_OUT_OF_PLACE,
      FFTW_MEASURE | FFTW_DESTROY_INPUT);

  /** init nodes from grid*/
  gridfcn(T, S, x, w);
  for (j = 0; j < my_nfft_plan.M_total; j++)
  {
    my_nfft_plan.x[2 * j + 0] = x[2 * j + 0];
    my_nfft_plan.x[2 * j + 1] = x[2 * j + 1];
  }

  /** precompute psi, the entries of the matrix B */
  if (my_nfft_plan.flags & PRE_LIN_PSI)
    NFFT(precompute_lin_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_PSI)
    NFFT(precompute_psi)(&my_nfft_plan);

  if (my_nfft_plan.flags & PRE_FULL_PSI)
    NFFT(precompute_full_psi)(&my_nfft_plan);

  /** init Fourier coefficients from given image */
  for (k = 0; k < my_nfft_plan.N_total; k++)
    my_nfft_plan.f_hat[k] = f[k] + _Complex_I * NFFT_K(0.0);

  /** NFFT-2D */
  NFFT(trafo)(&my_nfft_plan);

  /** FFTW-1Ds */
  for (t = 0; t < T; t++)
  {
    fft[0] = NFFT_K(0.0);
    for (r = -S / 2 + 1; r < S / 2; r++)
      fft[r + S / 2] = KERNEL(r) * my_nfft_plan.f[t * S + (r + S / 2)];

    NFFT(fftshift_complex_int)(fft, 1, &S);
    FFTW(execute)(my_fftw_plan);
    NFFT(fftshift_complex_int)(fft, 1, &S);

    for (r = 0; r < S; r++)
      Rf[t * S + r] = NFFT_M(creal)(fft[r]) / (NFFT_R)(S);

    /*    for(r=0; r<R/2; r++)
     Rf[t*R+(r+R/2)] = creal(cexp(-I*NFFT_KPI*r)*fft[r]);
     for(r=0; r<R/2; r++)
     Rf[t*R+r] = creal(cexp(-I*NFFT_KPI*r)*fft[r+R/2]);
     */
  }

  /** finalise the plans and free the variables */
  FFTW(destroy_plan)(my_fftw_plan);
  NFFT(free)(fft);
  NFFT(finalize)(&my_nfft_plan);
  NFFT(free)(x);
  NFFT(free)(w);
  return 0;
}

/** simple test program for the discrete Radon transform
 */
int main(int argc, char **argv)
{
  int (*gridfcn)(); /**< grid generating function        */
  int T, S; /**< number of directions/offsets    */
  FILE *fp;
  int N; /**< image size                      */
  NFFT_R *f, *Rf;

  if (argc != 5)
  {
    printf("radon gridfcn N T R\n");
    printf("\n");
    printf("gridfcn    \"polar\" or \"linogram\" \n");
    printf("N          image size NxN            \n");
    printf("T          number of slopes          \n");
    printf("R          number of offsets         \n");
    exit(EXIT_FAILURE);
  }

  if (strcmp(argv[1], "polar") == 0)
    gridfcn = polar_grid;
  else
    gridfcn = linogram_grid;

  N = atoi(argv[2]);
  T = atoi(argv[3]);
  S = atoi(argv[4]);
  /*printf("N=%d, %s grid with T=%d, R=%d. \n",N,argv[1],T,R);*/

  f = (NFFT_R *) NFFT(malloc)((size_t)(N * N) * (sizeof(NFFT_R)));
  Rf = (NFFT_R *) NFFT(malloc)((size_t)(T * S) * (sizeof(NFFT_R)));

  /** load data */
  fp = fopen("input_data.bin", "rb");
  if (fp == NULL)
    return EXIT_FAILURE;
  fread(f, sizeof(NFFT_R), (size_t)(N * N), fp);
  fclose(fp);

  /** Radon transform */
  Radon_trafo(gridfcn, T, S, f, N, Rf);

  /** write result */
  fp = fopen("sinogram_data.bin", "wb+");
  if (fp == NULL)
    return EXIT_FAILURE;
  fwrite(Rf, sizeof(NFFT_R), (size_t)(T * S), fp);
  fclose(fp);

  /** free the variables */
  NFFT(free)(f);
  NFFT(free)(Rf);

  return EXIT_SUCCESS;
}
