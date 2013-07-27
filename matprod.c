/* MATPROD - A LIBRARY FOR MATRIX MULTIPLICATION WITH OPTIONAL PIPELINING
             C Procedures for Matrix Multiplication Without Pipelining

   Copyright (c) 2013 Radford M. Neal.

   The matprod library is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include "matprod.h"


/* Dot product of two vectors of length k. 

   Two implementations are provided.  One uses the obvious loop, which
   maybe the compiler will optimize well.  In the other, the loop is
   unrolled to do two pairs of elements each iteration, with the sum
   initialized either to zero or to the result from the first pair, if
   the number of pairs is odd. 

   Use -DALT_MULT_VEC_VEC to switch between these two implementations.
   Change #ifdef to #ifndef or vice versa below to change the default. */

double matprod_vec_vec (double *x, double *y, int k)
{
#   ifdef ALT_MULT_VEC_VEC

        double s;
        int i;

        s = 0.0;

        for (i = 0; i<k; i++)
            s += x[i]*y[i];

        return s;

#   else

        double s;

        /* If k is odd, initialize sum to the first product, and adjust x,
           y, and k to account for this.  If k is even, just initialize
           sum to zero. */

        if (k & 1) {
            s = *x++ * *y++;
            k -= 1;
        }
        else
            s = 0.0;

        /* Add two products each time around loop, adjusting x, y, and k as
           we go.  Note that k will be even when we start. */

        while (k > 0) {
            s += *x++ * *y++;
            s += *x++ * *y++;
            k -= 2;
        }

        return s;

#   endif
}


/* Product of row vector (x) of length k and k x m matrix (y), result stored 
   in z. 

   The inner loop is a pair of vector dot products, each implemented 
   similarly to the matprod_vec_vec routine above.  As there, two 
   implementations are provided, one with loop unrolling within each
   dot product, the other without (since maybe the compiler does a
   better job of this).  The loop unrolling to do two dot products at
   one time is done manually in both implementations.

   Use -DALT_MULT_VEC_MAT to switch between these two implementations.
   Change #ifdef to #ifndef or vice versa below to change the default. */

void matprod_vec_mat (double *x, double *y, double *z, int k, int m)
{
    double s, s2, t;
    double *p, *y2;
    int i;

    /* If m is odd, compute the first element of the result (the dot product
       of x and the first column of y).  Adjust y, z, and m to account for 
       having handled the first column. */

    if (m & 1) {

#       ifdef ALT_MULT_VEC_MAT

            s = 0.0;

            for (i = 0; i<k; i++)
                s += x[i] * y[i];

            y += k;

#       else

            p = x;
            i = k;

            /* Initialize sum to first product, if k odd; otherwise to 0. */

            if (i & 1) {
                s = *p++ * *y++;
                i -= 1;
            }
            else
                s = 0.0;

            /* Add two products each time around loop. Note: i is even. */

            while (i > 0) {
                s += *p++ * *y++;
                s += *p++ * *y++;
                i -= 2;
            }

#       endif

        /* Store result of dot product as first element, decrement m. */

        *z++ = s;
        m -= 1;
    }

    /* In this loop, compute two consecutive elements of the result vector,
       by doing two dot products of x with columns of y.  Adjust y, z, and
       m as we go.  Note that m, the number of columns left to do, is even 
       when we start. */

    while (m > 0) {

        /* Set y and y2 to point to the starts of the two columns.  Note
           that y was modified in the previous dot product operation 
           so that it is now pointing at the next column to do. */

        y2 = y + k;

#       ifdef ALT_MULT_VEC_MAT

            s2 = s = 0.0;

            /* Each time around this loop, add one product for each of the 
               two dot products. */

            for (i = 0; i<k; i++) {
                t = x[i];
                s += t * y[i];
                s2 += t * y2[i];
            }

            y = y2 + k;

#       else

            /* Set p to point initially to x, and i to initially be k.  Both
               will be changed as the two dot products are computed. */

            p = x;
            i = k;

            /* If the two dot products sum an odd number of products, set
               the sums, s and s2, to the first products here, and adjust p, 
               y, y2, and i.  Otherwise, initialize s and s2 to zero. */

            if (i & 1) {
                t = *p++;
                s = t * *y++;
                s2 = t * *y2++;
                i -= 1;
            }
            else
                s2 = s = 0.0;

            /* Each time around this loop, add two products for each of the 
               two dot products, adjusting p, y, y2, and i as we go.  Note
               that i will be even when we start. */

            while (i > 0) {
                t = *p++;
                s += t * *y++;
                s2 += t * *y2++;
                t = *p++;
                s += t * *y++;
                s2 += t * *y2++;
                i -= 2;
            }

            y = y2;

#       endif

        /* Store the two dot products in the result vector. */

        *z++ = s;
        *z++ = s2;

        m -= 2;
    }
}


/* Product of n x k matrix (x) and column vector of length k (y) with result 
   stored in z. 

   The product is computed using an outer loop that accumulates the sums for 
   all elements of the result vector, iterating over columns of x, in order
   to produce sequential accesses.  This loop is unrolled to accumulate from
   two columns of x at once, which probably reduces the number of memory
   accesses, and may give more potential to overlap accesses with computation.
   The result vector is initialized either to zeros or to the result from the 
   first column, if the number of columns is odd.  Order of summation is
   kept the same as the obvious method, for consistency of round-off errors.

   The case of n=2 may be handled specially, accumulating sums in two
   local variables rather than in the result vector, and then storing
   them in the result at the end.  Whether this is done can be controlled
   using -DALT_MULT_MAT_VEC.  Change #ifdef to #ifndef or vice versa below 
   to change the default. */

void matprod_mat_vec (double *x, double *y, double *z, int n, int k)
{
    double *p, *q;
    double b, b2;
    int i;

#   ifndef ALT_MULT_MAT_VEC

        if (n == 2) { 

            double s1, s2;

            /* Initialize s1 and s2 to zero, if k is even, or to the products
               of the first element of y with the first column of x.  Adjust 
               x, y, and k accordingly. */

            if (k & 1) {
                b = *y++;
                s1 = *x++ * b;
                s2 = *x++ * b;
                k -= 1;
            }
            else
                s1 = s2 = 0.0;

            /* Each time around this loop, add the products of two columns of
               x with two elements of y to s1 and s2.  Adjust x, y, and k to
               account for this.  Note that k will be even when we start. */

            while (k > 0) {
                b = *y++;
                b2 = *y++;
                s1 = (s1 + (x[0] * b)) + (x[2] * b2);
                s2 = (s2 + (x[1] * b)) + (x[3] * b2);
                x += 4;
                k -= 2;
            }

            /* Store s1 and s2 in the result vector. */

            z[0] = s1;
            z[1] = s2;

            return;
        }

#   endif

    /* Initialize sums in z to zero, if k is even, or to the product of
       the first element of y with the first column of x.  Adjust x, y,
       and k accordingly. */

    q = z;

    if (k & 1) {
        b = *y++;
        for (i = n; i > 0; i--)
            *q++ = *x++ * b;
        k -= 1;
    }
    else 
        for (i = n; i > 0; i--)
            *q++ = 0.0;

    /* Each time around this loop, add the products of two columns of x 
       with two elements of y to the result vector, z.  Adjust x, y, and
       k to account for this.  Note that k will be even when we start. */

    while (k > 0) {
        p = x + n;
        q = z;
        b = *y++;
        b2 = *y++;
        for (i = n; i > 0; i--) {
            double tmp = *q + (*x++ * b);  /* ensure order of addition */
            *q++ = tmp + (*p++ * b2);
        }
        x = p;
        k -= 2;
    }
}


/* Product of an n x k matrix (x) and a k x m matrix (y) with result stored 
   in z. 

   The inner loop does two matrix-vector products each time, implemented 
   much as in matprod_mat_vec above, except for computing two columns. This
   gives a reasonably efficient implementation of an outer product (where
   k is one). Order of summation is kept the same as the obvious method, 
   for consistency of round-off errors.

   The case of n=2 may be handled specially, accumulating sums in two
   local variables rather than in a column of the result, and then storing
   them in the result column at the end.  Whether this is done can be 
   controlled using -DALT_MULT_MAT_MAT.  Change #ifdef to #ifndef or 
   vice versa below to change the default. */

void matprod (double *x, double *y, double *z, int n, int k, int m)
{
    int i, j;

#   ifndef ALT_MULT_MAT_MAT

        if (n == 2) {
    
            /* If m is odd, compute the first column of the result, and
               update y, z, and m accordingly. */
    
            if (m & 1) {
    
                double s1, s2;
                double *r;
    
                r = x;   /* r set to x, and then modified */
                j = k;   /* j set to k, and then modified */
    
                /* Initialize s1 and s2 to zero, if k is even, or to the 
                   products of the first element of the next column of y with
                   the first column of x.  Adjust x, y, and j accordingly. */
    
                if (j & 1) {
                    double b = *y++;
                    s1 = *r++ * b;
                    s2 = *r++ * b;
                    j -= 1;
                }
                else
                    s1 = s2 = 0.0;
    
                /* Each time around this loop, add the products of two columns
                   of x and two elements of the next column of y to s1 and s2.
                   Adjust x, y, and j to account for this.  Note that j will 
                   be even when we start. */
    
                while (j > 0) {
                    double b1, b2;
                    b1 = *y++;
                    b2 = *y++;
                    s1 = (s1 + (r[0] * b1)) + (r[2] * b2);
                    s2 = (s2 + (r[1] * b1)) + (r[3] * b2);
                    r += 4;
                    j -= 2;
                }
    
                /* Store s1 and s2 in the result column. */
    
                z[0] = s1;
                z[1] = s2;
    
                /* Move to next column of the result. */
    
                z += 2;
                m -= 1;
    
            }
    
            /* Compute two columns of the result each time around this loop, 
               updating y, z, and m accordingly. */
    
            while (m > 0) {
    
                double s11, s12, s21, s22;
                double *y2 = y + k;
                double *r;
    
                r = x;   /* r set to x, and then modified */
                j = k;   /* j set to k, and then modified */
    
                /* Initialize sums for columns to zero, if k is even, or to the 
                   products of the first element of the next column of y with
                   the first column of x.  Adjust x, y, and j accordingly. */
    
                if (j & 1) {
                    double b = *y++;
                    s11 = r[0] * b;
                    s12 = r[1] * b;
                    double b2 = *y2++;
                    s21 = r[0] * b2;
                    s22 = r[1] * b2;
                    r += 2;
                    j -= 1;
                }
                else
                    s11 = s12 = s21 = s22 = 0.0;
    
                /* Each time around this loop, add the products of two columns
                   of x with two elements of the next two columns of y to the 
                   sums.  Adjust r, y, and j to account for this.  Note that 
                   j will be even. */
    
                while (j > 0) {
                    double b11 = *y++;
                    double b12 = *y++;
                    double b21 = *y2++;
                    double b22 = *y2++;
                    s11 = (s11 + (r[0] * b11)) + (r[2] * b12);
                    s12 = (s12 + (r[1] * b11)) + (r[3] * b12);
                    s21 = (s21 + (r[0] * b21)) + (r[2] * b22);
                    s22 = (s22 + (r[1] * b21)) + (r[3] * b22);
                    r += 4;
                    j -= 2;
                }
    
                /* Store sums in the next two result columns. */
    
                z[0] = s11;
                z[1] = s12;
                z[2] = s21;
                z[3] = s22;

                /* Move to next column of the result. */

                y = y2;
                z += 4;
                m -= 2;
    
            }
    
            return;
    
        }

#   endif

    /* If m is odd, compute the first column of the result, updating y, z, and 
       m to account for this column having been computed (so that the situation
       is the same as if m had been even to start with). */

    if (m & 1) {

        double *r;

        r = x;   /* r set to x, and then modified as columns of x are summed */
        j = k;   /* j set to k, and then modified as values are read from y */

        /* Initialize sums in z to zero, if k is even, or to the product of
           the first element of the next column of y with the first column 
           of x (in which case adjust r, y, and j accordingly). */

        if (j & 1) {
            double *q = z;
            double b = *y++;
            for (i = n; i > 0; i--)
                *q++ = *r++ * b;
            j -= 1;
        }
        else {
            double *q = z;
            for (i = n; i > 0; i--)
                *q++ = 0;
        }

        /* Each time around this loop, add the products of two columns of x 
           with two elements of the next column of y to the result vector, z.
           Adjust r, y, and j to account for this.  Note that j will be even 
           when we start. */

        while (j > 0) {
            double *q = z;
            double *r2 = r + n;
            double b = *y++;
            double b2 = *y++;
            for (i = n; i > 0; i--) {
                *q =  (*q + (*r++ * b)) + (*r2++ * b2);
                q += 1;
            }
            r = r2;
            j -= 2;
        }

        /* Move to next column of the result. */

        z += n;
        m -= 1;
    }

    /* Compute two columns of the result each time around this loop, updating
       y, z, and m accordingly.  Note that m will be even.  (At the start
       of each loop iteration, the work remaining to be done is the same as 
       if y, z, and m (and x, n, and k, which don't change) had been the 
       original arguments.) */

    while (m > 0) {

        double *y2 = y + k;
        double *r;

        r = x;   /* r set to x, and then modified as columns of x are summed */
        j = k;   /* j set to k, and then modified as values are read from y */

        /* Initialize sums in next two columns of z to zero, if k is even, 
           or to the products of the first elements of the next two columns
           of y with the first column of x, if k is odd (in which case adjust 
           r, y, and j accordingly). */

        if (j & 1) {
            double *q = z;
            double b = *y++;
            for (i = n; i > 0; i--)
                *q++ = *r++ * b;
            double b2 = *y2++;
            r = x;
            for (i = n; i > 0; i--)
                *q++ = *r++ * b2;
            j -= 1;
        }
        else {
            double *q = z;
            for (i = 2*n; i > 0; i--)
                *q++ = 0;
        }

        /* Each time around this loop, add the products of two columns of x 
           with two elements of the next two columns of y to the next two
           columns of the result vector, z.  Adjust r, y, and j to account 
           for this.  Note that j will be even. */

        while (j > 0) {
            double *r2 = r + n;
            double *q1 = z;
            double *q2 = z + n;
            double b11 = *y++;
            double b12 = *y++;
            double b21 = *y2++;
            double b22 = *y2++;
            for (i = n; i > 0; i--) {
                double t = *r;
                double t2 = *r2;
                *q1 = (*q1 + (t * b11)) + (t2 * b12);
                q1 += 1;
                *q2 = (*q2 + (t * b21)) + (t2 * b22);
                q2 += 1;
                r += 1;
                r2 += 1;
            }
            r = r2;
            j -= 2;
        }

        /* Move to the next two columns. */

        y = y2;
        z += 2*n;
        m -= 2;
    }
}