#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H


unsigned long long hash(const void *data, size_t size);

void state_dump(unsigned int me, const void *state, unsigned int size);

#endif //_FUNCTIONS_H
