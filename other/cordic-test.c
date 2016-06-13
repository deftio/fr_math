#include "cordic-32bit.h"
#include <math.h> // for testing only!

//Print out sin(x) vs fp CORDIC sin(x)
int main(int argc, char **argv)
{
    double p;
    int s,c;
    int i;    
    for(i=0;i<50;i++)
    {
        p = (i/50.0)*M_PI/2;        
        //use 32 iterations
        cordic((p*MUL), &s, &c, 32);
        //these values should be nearly equal
        printf("%f : %f\n", s/MUL, sin(p));
    }       
}