// Functions around homographies

/* This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Pulic License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2014-2019, Thibaud Briand <thibaud.briand@enpc.fr>
 *
 * All rights reserved.
 *
 */

#ifndef HOMOGRAPHY_CORE
#define HOMOGRAPHY_CORE

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "random.h"
#include "parsenumbers.h"
#include "cmphomod.h"

void read_homography(double *homo, char *characters) {
    int maxparam = 9;
    int nparams = parse_doubles(homo, maxparam, characters);
    assert(nparams <= maxparam);
}

void read_homography_file(double *homo, char *filename) {
    FILE *f = fopen(filename, "r");
    int maxparam = 9;
    int nparams;
    double *homo_tmp = read_ascii_doubles(f, &nparams);
    fclose(f);
    assert(nparams <= maxparam);
    for (int i=0; i<nparams; i++)
        homo[i] = homo_tmp[i];
    free(homo_tmp);
}

// Compute the inverse of an homography.
// The 3x3 matrix is inverted using closed-form formulas.
void invert_homography(double iH[9], const double H[9])
{
  double det = H[0] * (H[4]*H[8] - H[5] * H[7]);
  det -= H[1] * (H[3]*H[8] - H[5] * H[6]);
  det += H[2] * (H[3]*H[7] - H[4] * H[6]);

  double tmp = 1.0/det;

  iH[0] = tmp * (H[4] * H[8] - H[5] * H[7]);
  iH[3] = tmp * (H[5] * H[6] - H[3] * H[8]);
  iH[6] = tmp * (H[3] * H[7] - H[4] * H[6]);

  iH[1] = tmp * (H[2] * H[7] - H[1] * H[8]);
  iH[4] = tmp * (H[0] * H[8] - H[2] * H[6]);
  iH[7] = tmp * (H[1] * H[6] - H[0] * H[7]);

  iH[2] = tmp * (H[1] * H[5] - H[2] * H[4]);
  iH[5] = tmp * (H[2] * H[3] - H[0] * H[5]);
  iH[8] = tmp * (H[0] * H[4] - H[1] * H[3]);
}

// Compute the image y of the vector x by the homography H.
void apply_homography(double y[2], const double x[2], const double H[9])
{
  double z[3];
  z[0] = H[0]*x[0] + H[1]*x[1] + H[2];
  z[1] = H[3]*x[0] + H[4]*x[1] + H[5];
  z[2] = H[6]*x[0] + H[7]*x[1] + H[8];
  y[0] = z[0]/z[2];
  y[1] = z[1]/z[2];
}

// translate homography in order to compute the result of a crop
// h_in(x+tx,y+ty) = (tx,ty) + h_out(x,y)
// i.e h_out = t(-tx,-ty) o h_in o t(tx,ty)
void translate_homography(double *h_out, double *h_in, double tx, double ty)
{
    h_out[8] = h_in[6]*tx + h_in[7]*ty + h_in[8];
    h_out[0] = h_in[0] - tx*h_in[6];
    h_out[1] = h_in[1] - tx*h_in[7];
    h_out[2] = (h_in[0]*tx + h_in[1]*ty + h_in[2]) - tx*h_out[8];
    h_out[3] = h_in[3] - ty*h_in[6];
    h_out[4] = h_in[4] - ty*h_in[7];
    h_out[5] = (h_in[3]*tx + h_in[4]*ty + h_in[5]) - ty*h_out[8];
    h_out[6] = h_in[6];
    h_out[7] = h_in[7];
}

// zoom homography in order to be compatible with a zoom of an image
// (zoomx, zoomy) h_in (x/zoomx,y/zoomy) = h_out(x,y)
void zoom_homography(double *h_out, double *h_in, double zx, double zy)
{
    h_out[0] = h_in[0];
    h_out[1] = h_in[1]*zx*1.0/zy;
    h_out[2] = h_in[2]*zx;
    h_out[3] = h_in[3]*zy*1.0/zx;
    h_out[4] = h_in[4];
    h_out[5] = h_in[5]*zy;
    h_out[6] = h_in[6]/zx;
    h_out[7] = h_in[7]/zy;
    h_out[8] = h_in[8];
}

void create_random_translation(double H[9], double L)
{
        double tx = (2*L*random_uniform() - L);
        double ty = (2*L*random_uniform() - L);
        H[0] = 1.0;
        H[1] = 0.0;
        H[2] = tx;
        H[3] = 0.0;
        H[4] = 1.0;
        H[5] = ty;
        H[6] = 0.0;
        H[7] = 0.0;
        H[8] = 1.0;
}

void create_random_homography(double H[9], int w, int h, double L)
{
    double corner[4][2] = {{0,0}, {w-1,0}, {0,h-1}, {w-1,h-1}};
    double corner2[4][2];
    double a;
    
    for(int j = 0; j < 2; j++)
        for(int i = 0; i < 4; i++) {
            a = random_uniform();
            corner2[i][j] = corner[i][j] + (2*L*a - L);
        }

    double R[3][3];
    homography_from_4corresp(
    corner[0], corner[1], corner[2], corner[3],
    corner2[0], corner2[1], corner2[2], corner2[3], R);

    for(int i = 0; i < 9; i++) 
        H[i] = R[i/3][i%3];
}

void create_random_transformation(double H[9], double L, int w, int h, int type)
{
    if( type == 2 )
        create_random_translation(H, L);
    else if ( type == 3 ) {
        create_random_translation(H, L);
        double theta = 5*M_PI/180*(2*random_uniform()-1); //rotation of at most 5°
        int w2 = (w+1)/2;
        int h2 = (h+1)/2;
        H[0] = cos(theta);
        H[1] = -sin(theta);
        H[3] = -H[1];
        H[4] = H[0];
        H[2] += w2*(1-H[0])-h2*H[1];
        H[5] += -w2*H[3]+h2*(1-H[4]);
    }
    else if ( type == 6 ) {
        create_random_homography(H, w, h, L);
        H[6] = 0.0;
        H[7] = 0.0;
    }
    else { // type = 8 (or invalid type)
        create_random_homography(H, w, h, L);
    }
}

// compute the field as norm(homo2^{-1}( homo1( x ) ) - x)
void compute_field_inv(float *field, double *homo1, double *homo2, int w, int h)
{
    double x[2];
    double y[2];
    double z[2];
    double ihomo2[9];
  
    invert_homography(ihomo2, homo2);
    
    for( int j = 0; j < h; j++) {
        x[1] = j;
        for( int i = 0; i < w; i++ ) {
            // initialisation
            x[0] = i;
      
            // calcul de h2^-1 \circ h - Id
            apply_homography(y, x, homo1);
            apply_homography(z, y, ihomo2);
            z[0] -= x[0];
            z[1] -= x[1];
      
            // fill the output image
            field[i+j*w] = hypot(z[0],z[1]);
        }
    }
}

// compute the field as norm(homo2(x) - homo1(x))
void compute_field(float *field, double *homo1, double *homo2, int w, int h)
{
    double x[2];
    double z1[2];
    double z2[2];

    for( int j = 0; j < h; j++) {
        x[1] = j;
        for( int i = 0; i < w; i++ ) {
            // initialisation
            x[0] = i;
      
            // calcul de h2^-1 \circ h - Id
            apply_homography(z1, x, homo1);
            apply_homography(z2, x, homo2);
            
            // fill the output image
            field[i+j*w] = hypot(z1[0]-z2[0],z1[1]-z2[1]);
        }
    }
}

#endif
