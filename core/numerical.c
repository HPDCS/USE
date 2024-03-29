/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file numerical.c
* @brief In this module there are the implementations of Piece-Wise Deterministic
*        numerical distribution implementations
* @author Alessandro Pellegrini
* @date 3/16/2011
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <ROOT-Sim.h>

#include <dymelor.h>
#include <core.h>
#include <numerical.h>


static seed_type master_seed;
//static __thread seed_type thread_seed = 0;


/**
* This function returns a number in between (0,1), according to a Uniform Distribution.
* It is based on Multiply with Carry by George Marsaglia
*
* @author Alessandro Pellegrini
* @return A random number, in between (0,1)
* @date 05 sep 2013
*/
double Random(void) {

	uint32_t *seed1;
	uint32_t *seed2;

	//UPDATED


	//if(thread_seed == 0)
	//	thread_seed = master_seed;
	//
	//seed1 = (uint32_t *)&thread_seed;
	//seed2 = (uint32_t *)((char *)&thread_seed + (sizeof(uint32_t)));

	if(rootsim_config.serial) {
		seed1 = (uint32_t *)&master_seed;
		seed2 = (uint32_t *)((char *)&master_seed + (sizeof(uint32_t)));
	} else {
		seed1 = (uint32_t *)&(LPS[current_lp]->seed);
		seed2 = (uint32_t *)((char *)&(LPS[current_lp]->seed) + (sizeof(uint32_t)));
	}

	*seed1 = 36969u * (*seed1 & 0xFFFFu) + (*seed1 >> 16u);
	*seed2 = 18000u * (*seed2 & 0xFFFFu) + (*seed2 >> 16u);

	// The magic number below is 1/(2^32 + 2).
    	// The result is strictly between 0 and 1.
	return (((*seed1 << 16u) + (*seed1 >> 16u) + *seed2) + 1.0) * 2.328306435454494e-10;
}





int RandomRange(int min, int max) {
	return (int)floor(Random() * (max - min + 1)) + min;
}



int RandomRangeNonUniform(int x, int min, int max) {
      return (((RandomRange(0, x) | RandomRange(min, max))) % (max - min + 1)) + min;
}

/**
* This function returns a random number according to an Exponential distribution.
* The mean value of the distribution must be passed as the mean value.
*
* @author Alessandro Pellegrini
* @param mean Mean value of the distribution
* @return A random number
* @date 3/16/2011
*/
double Expent(double mean) {

	if(mean < 0) {
		fprintf(stderr, "Error: in call to Expent() passed a negative mean value\n");
		abort();
	}

	//return (-mean * log(1 - Random()));
	return mean * ( 1.0 / ( 3 *(1.01 - Random()) ) );
}





/**
* This function returns a number according to a Normal Distribution with mean 0
*
* @author
* @return A random number
* @date 4/20/2011
*/
double Normal(void) {
	// TODO: usare queste variabili statiche non garantisce necessariamente l'esecuzione PWD se c'è interleaving tra gli LP che chiamano Normal()
	static bool iset = false;
	static double gset;
	double fac, rsq, v1, v2;

	if(iset == false) {
		do {
			v1 = 2.0 * Random() - 1.0;
			v2 = 2.0 * Random() - 1.0;
			rsq = v1 * v1 + v2 * v2;
		} while(rsq >= 1.0 || D_DIFFER_ZERO(rsq));

		fac = sqrt(-2.0 * log(rsq) / rsq);

		// Perform Box-Muller transformation to get two normal deviates. Return one
		// and save the other for next time.
		gset = v1 * fac;
		iset = true;
		return v2 * fac;
	} else {
		// A deviate is already available
		iset = false;
		return gset;
	}
}




/**
* This function returns a number in according to a Gamma Distribution of Integer Order ia,
* a waiting time to the ia-th event in a Poisson process of unit mean.
*
* @author D. E. Knuth
* @param ia Integer Order of the Gamma Distribution
* @return A random number
* @date 4/20/2011
*/
double Gamma(int ia) {
	int j;
	double am, e, s, v1, v2, x, y;

	if(ia < 1) {
		rootsim_error(false, "Gamma distribution must have a ia value >= 1. Defaulting to 1...");
		ia = 1;
	}

	if(ia < 6) {
		// Use direct method, adding waiting times
		x = 1.0;
		for(j = 1; j <= ia; j++)
			x *= Random();
		x = -log(x);
	} else {
		// Use rejection method
		do {
			do {
				do {
					v1 = Random();
					v2 = 2.0 * Random() - 1.0;
				} while(v1 * v1 + v2 * v2 > 1.0);

				y = v2 / v1;
				am = (double)(ia - 1);
				s = sqrt(2.0 * am + 1.0);
				x = s * y + am;
			} while(x < 0.0);

			e = (1.0 + y * y) * exp(am * log(x / am) - s * y);
		} while(Random() > e);
	}

	return x;
}



/**
* This function returns the waiting time to the next event in a Poisson process of unit mean.
*
* @author Alessandro Pellegrini
* @param ia Integer Order of the Gamma Distribution
* @return A random number
* @date 11 Jan 2012
*/
double Poisson(void) {
	return Gamma(1);
}



/**
* This function returns a random sample from a Zipf distribution.
* Based on the rejection method by Luc Devroye for sampling:
* "Non-Uniform Random Variate Generation, page 550, Springer-Verlag, 1986
*
* @author Alessandro Pellegrini
* @param skew The skew of the distribution
* @param limit The largest sample to retrieve
* @return A random number
* @date 8 Nov 2012
*/
int Zipf(double skew, int limit) {
	double a = skew;
	double b = pow(2., a - 1.);
	double x, t, u, v;
	do {
		u = Random();
		v = Random();
		x = floor(pow(u, -1./a - 1.));
		t = pow(1. + 1./x, a - 1.);
	} while (v * x * (t - 1.) / (b - 1.) > (t/b) || x > limit);
	return (int)x;
}



/**
* MwC random number generators suffer from bad seeds. Since initial LPs' seeds are derived randomly
* from a random master seed, chances are that we incur in a bad seed. This would create a strong
* bias in the generation of random numbers for certain LPs.
* This function checks whether a randomly generated seed is a bad one, and switch to a different
* safe initial state.
* This generation is deterministic, therefore it is good for PWD executions of different simulations
* using the same initial master seed.
*
* @author Alessandro Pellegrini
* @param cur_seed The current seed which has been generated and must be checked
* @return A sanitized seed
* @date 6 Sep 2013
*/

seed_type sanitize_seed(seed_type cur_seed) {

        uint32_t *seed1 = (uint32_t *)&(cur_seed);
        uint32_t *seed2 = (uint32_t *)((char *)&(cur_seed) + (sizeof(uint32_t)));

        // Sanitize seed1
        // Any integer multiple of 0x9068FFFF, including 0, is a bad state
        uint32_t state_orig;
        uint32_t temp;

        state_orig = *seed1;
        temp = state_orig;

        // The following is equivalent to % 0x9068FFFF, without using modulo
        // operation which may be expensive on embedded targets. For
        // uint32_t and this divisor, we only need 'if' rather than 'while'.
        if (temp >= UINT32_C(0x9068FFFF))
                temp -= UINT32_C(0x9068FFFF);
        if (temp == 0) {
                // Any integer multiple of 0x9068FFFF, including 0, is a bad state.
                // Use an alternate state value by inverting the original value.
                temp = state_orig ^ UINT32_C(0xFFFFFFFF);
                if (temp >= UINT32_C(0x9068FFFF))
                        temp -= UINT32_C(0x9068FFFF);
        }
        *seed1 = temp;


        // Sanitize seed2
        // Any integer multiple of 0x464FFFFF, including 0, is a bad state.
        state_orig = *seed2;
        temp = state_orig;

        // The following is equivalent to % 0x464FFFFF, without using modulo
        // operation which may be expensive on embedded targets. For
        // uint32_t and this divisor, it may loop up to 3 times.
        while (temp >= UINT32_C(0x464FFFFF))
                temp -= UINT32_C(0x464FFFFF);
        if (temp == 0) {
                // Any integer multiple of 0x464FFFFF, including 0, is a bad state.
                // Use an alternate state value by inverting the original value.
                temp = state_orig ^ UINT32_C(0xFFFFFFFF);
                while (temp >= UINT32_C(0x464FFFFF))
                        temp -= UINT32_C(0x464FFFFF);
        }

        *seed2 = temp;

        return cur_seed;
}







/**
* This function is used by ROOT-Sim to load the initial value of the seed.
*
* A configuration file is used to store the last master seed, amongst different
* invocations of the simulator. Using the first value stored in the file, a random
* number is generated, which is used as a row-offset in the file istelf, where a
* specified seed will be found.
* The first line is the replaced with the subsequent seed generated by the Random
* function.
*
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @return The master seed
*/
static void load_seed(void) {

	seed_type new_seed;
	char conf_file[512];
	FILE *fp;
	int read_bytes=0;

	// Get the path to the configuration file
	sprintf(conf_file, "%s/.rootsim/numerical.conf", getenv("HOME"));

	// Check if the file exists. If not, we have to create configuration
	if ((fp = fopen(conf_file, "r+")) == NULL) {

		// Try to build the path to the configuration folder.
		sprintf(conf_file, "%s/.htmpdes", getenv("HOME"));
		_mkdir(conf_file);

		// Create and initialize the file
		sprintf(conf_file, "%s/.htmpdes/numerical.conf", getenv("HOME"));
		if ((fp = fopen(conf_file, "w")) == NULL) {
			rootsim_error(true, "Unable to create the numerical library configuration file %s. Aborting...", conf_file);
		}

		// We now initialize the first long random number. Thanks Unix!
		// TODO: THIS IS NOT PORTABLE!
		int fd;
		if ((fd = open("/dev/urandom", O_RDONLY)) == -1) {
			rootsim_error(true, "Unable to initialize the numerical library configuration file %s. Aborting...", conf_file);

		}
		read_bytes = read(fd, &new_seed, sizeof(seed_type));
		if(read_bytes != sizeof(seed_type))
			rootsim_error(true, "Unable to read from urandom");

		close(fd);
		fprintf(fp, "%llu\n", (unsigned long long)new_seed); // We cast, so that we get an integer representing just a bit sequence
		fclose(fp);

	}

	// Load the configuration for the numerical library
	if ((fp = fopen(conf_file, "r+")) == NULL) {
		rootsim_error(true, "Unable to load numerical distribution configuration: %s. Aborting...", conf_file);
	}


	// Load the initial seed
	fscanf(fp, "%llu", (unsigned long long *)&master_seed);


	rewind(fp);
	srandom(master_seed);
//	new_seed = random();
//	fprintf(fp, "%llu\n", (unsigned long long)new_seed);

	fclose(fp);
}


// TODO: with a high number of LPs (greater that the number of bit of an integer), the shift return the same seed to multiple LP.
//This means that two LPs should produce same events with the same ts!!!

#define RS_WORD_LENGTH (8 * sizeof(seed_type))
#define ROR(value, places) (value << (places)) | (value >> (RS_WORD_LENGTH - places)) // Circular shift
void numerical_init(void) {

	// Initialize the master seed
	load_seed();
	master_seed = sanitize_seed(master_seed);

}
#undef RS_WORD_LENGTH
#undef ROR




void numerical_fini(void) {
}

