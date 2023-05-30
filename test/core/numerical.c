#define _GNU_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <datatypes/bitmap.h>
#include <simtypes.h>
#include <ROOT-Sim.h>


#define ITERATIONS 100000
#define BITMAP_ENTRIES 10000

#define NO_ERROR		 		 0
#define ACTUAL_LEN_ERROR 		 1
#define NON_ZERO_BYTE_AFTER_INIT 2
#define UNVALID_GET_BIT 		 3
#define VIRTUAL_LEN_ERROR 		 4

static char messages[5][256] = {
	"none",
	"actual_len < virtual_len",
	"non zero byte after init",
	"get_bit returned an unxpected value",
	"BITMAP_ENTRIES != virtual_len"
};



#define MAX_PATHLEN			512
#include <stdarg.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>

simulation_configuration pdes_config;
__thread unsigned int current_lp = 0;
LP_state **LPS = NULL;


// LCOV_EXCL_START
void rootsim_error(bool fatal, const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	fprintf(stderr, (fatal ? "[FATAL ERROR] " : "[WARNING] "));

	fprintf(stderr, "%s", buf);
	fflush(stderr);

    assert(0);    
}

void _mkdir(const char *path) {

	char opath[MAX_PATHLEN];
	char *p;
	size_t len;

	strncpy(opath, path, MAX_PATHLEN-1);
	len = strlen(opath);
	if (opath[len - 1] == '/')
		opath[len - 1] = '\0';

	// Path plus 1 is a hack to allow also absolute path
	for (p = opath + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (access(opath, F_OK))
				if (mkdir(opath, S_IRWXU))
					if (errno != EEXIST) {
						rootsim_error(true, "Could not create output directory", opath);
					}
			*p = '/';
		}
	}

	// Path does not terminate with a slash
	if (access(opath, F_OK)) {
		if (mkdir(opath, S_IRWXU)) {
			if (errno != EEXIST) {
				if (errno != EEXIST) {
					rootsim_error(true, "Could not create output directory", opath);
				}
			}
		}
	}
}

#include <numerical.c>

static int numerical_test(void)
{
	double res;
	res = Random(); 
	if(res < 0 && res > 1) return -1;
	res = RandomRange(0, 1);
	if(res < 0 && res > 1) return -1;
  	res = RandomRangeNonUniform(3, 0, 5);
	if(res < 0) return -1;
  	res = Expent(1.0);
	if(res < 0) return -1;
  	res = Normal();
	res = Gamma(1);
	if(res < 0) return -1;
  	res = Poisson();
	if(res < 0) return -1;
  	res = Zipf(0.5, 1000);
	if(res < 0) return -1;
  	return 0;
}

int main(void)
{
	pdes_config.serial = 1;
	numerical_init();
	printf("Testing numerical library implementation...");
	int res =  numerical_test();
	if(res == 0){
		printf("PASSED\n");
	}
	else
		printf("%s FAILED\n", messages[-res]);

	exit(res);
}
// LCOV_EXCL_STOP
