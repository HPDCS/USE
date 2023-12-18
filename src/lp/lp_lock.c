#include <core.h>

//used to take locks on LPs
volatile unsigned int *lp_lock;
__thread volatile unsigned int potential_locked_object  = UNDEFINED_LP; /// index of the lp locked in normal context

