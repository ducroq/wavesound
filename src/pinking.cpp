#include <Arduino.h>
#include "pinking.h"

bool pinkingFilter(float fs, int N, int *c)
/*
	Compute coefficients of a cascade of N first order sections
    that form a pinking filter.
	@param fs is the sampling frequency
	@param N is the cascade order
	@param s is a pointer to store the resulting coefficients
	returns true on success and false on failure
*/
{
    float d = cos(2*PI/(2*float(N)+1));

    for (int i = 0; i < N; i++)
	// approximate a pixelated line and mark vertical changes
    if (i == 0)
    {
        c[i] = d;
    }
    else if (i==1)
    {
        c[i] = 2*d*d - 1;
    } else
    {
        c[i] = 2*d*c[i-1] - c[i-2];
    }
    return true;
}
