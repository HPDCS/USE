
#include <math.h>
#include <stdlib.h>

double uniform_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num *= mean*2;
	return random_num;
}

double neg_triangular_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num = 1-sqrt(random_num);
	random_num *= mean*3/2;
	return random_num;
}

double triangular_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num = sqrt(random_num);
	random_num *= mean*3/2;
	return random_num;
}

double exponential_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num =  -log(random_num);
	random_num *= mean;
	return random_num;
}

