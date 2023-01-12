#ifndef __MATH_HPCDS__
#define __MATH_HPCDS__

extern double uniform_rand(struct drand48_data *seed, double mean);
extern double neg_triangular_rand(struct drand48_data *seed, double mean);
extern double triangular_rand(struct drand48_data *seed, double mean);
extern double exponential_rand(struct drand48_data *seed, double mean);































#endif
