#include <stdio.h>        // Provides for printf, etc...
#include <stdlib.h>       // Provides for exit, ...
#include <string.h>       // Provides for memset, ...
#include <configuration.h>

#include <argp.h>         // Provides GNU argp() argument parser


simulation_configuration rootsim_config;
simulation_configuration pdes_config;


const char *argp_program_version = "USE 1.0";
const char *argp_program_bug_address = "<romolo.marotta@gmail.com>";
static char doc[] = "USE -- The Ultimate Share-Everything PDES";
static char args_doc[] = "-c CORES -p LPS";  


// Order of fields: {NAME, KEY, ARG, FLAGS, DOC, GROUP}.
static struct argp_option options[] = {
  {"ncores",       'c', "CORES"   ,  0,  "Number of threads to be used"               , 0 },
  {"nprocesses",   'p', "LPS"     ,  0,  "Number of simulation objects"               , 0 },
  {"wall-timeout", 'w', "SECONDS" ,  0,  "End the simulation after SECONDS elapsed"   , 0 },
  {"verbose",      'v', 0         ,  0,  "Provide verbose output"                     , 0 },
  { 0, 0, 0, 0, 0, 0} 
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  // Figure out which option we are parsing, and decide
  // how to store it  
  switch (key)
    {
    case 'c':
      pdes_config.ncores = atoi(arg);
      break;
    case 'p':
      pdes_config.nprocesses = atoi(arg);
      break;
    case 'w':
      pdes_config.timeout = atoi(arg);
      break;
    case ARGP_KEY_END:
      if(pdes_config.ncores < 1)
      {
        printf("Please provide a valid number of cores to be used for simulation\n");
        argp_usage (state);
      }
      if(pdes_config.nprocesses < 1)
      {
        printf("Please provide a valid number of logical processes to be used for simulation\n");
        argp_usage (state);
      }
      break;
      
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

void parse_options(int argn, char **argv){
    argp_parse(&argp, argn, argv, 0, 0, NULL);
}