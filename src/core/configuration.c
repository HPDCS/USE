#include <stdio.h>        // Provides for printf, etc...
#include <stdlib.h>       // Provides for exit, ...
#include <string.h>       // Provides for memset, ...
#include <configuration.h>
#include <hpdcs_utils.h>
#include <argp.h>         // Provides GNU argp() argument parser


simulation_configuration pdes_config;


#define N_CORES_KEY                 'c'
#define N_PROCESSES_KEY             'p'
#define WALLCLOCK_TIMEOUT_KEY       'w' 

#define OBS_PERIOD_KEY              256

#define DISABLE_COMMITTER_KEY       257


#define ENFORCE_LOCALITY_KEY        356
#define EL_DYN_WINDOW_KEY           357
#define EL_WINDOW_SIZE_KEY          358
#define EL_LOCKED_PIPE_SIZE_KEY     359
#define EL_EVICTED_PIPE_SIZE_KEY    360
#define EL_TH_THRESHOLD_KEY         362
#define EL_WAIT_ROLLBACK_TH_KEY     363
#define EL_TH_THRESHOLD_CNT_KEY     364
#define ISS_ENABLED                 365
#define ISS_ENABLED_MPROTECTION     366
#define ISS_SIGNAL_MPROTECT         367

#define CKPT_PERIOD_KEY             461
#define CKPT_FOSSIL_PERIOD_KEY      462
#define CKPT_AUTONOMIC_PERIOD_KEY   463
#define CKPT_AUTOPERIOD_BOUND_KEY   464
#define CKPT_FORCED_FULL_PERIOD_KEY 465

#define NUMA_REBALANCE_KEY          564
#define ENABLE_MBIND_KEY            565
#define ENABLE_CUSTOM_ALLOC_KEY     566
#define SEGMENT_SCALE_KEY           567

#define ONGVT_PERIOD_KEY            667
#define ONGVT_MODE_KEY              668

#define DISTRIBUTED_FETCH_KEY       763
#define DISTRIBUTED_FETCH_BOUND_KEY 764


const char *argp_program_version = "USE 1.0";
const char *argp_program_bug_address = "<romolo.marotta@gmail.com>";
static char doc[] = "USE -- The Ultimate Share-Everything PDES";
static char args_doc[] = "-c CORES -p LPS";  


// Order of fields: {NAME, KEY, ARG, FLAGS, DOC, GROUP}.
static struct argp_option options[] = {
  {"ncores",              N_CORES_KEY              , "CORES"   ,  0                  ,  "Number of threads to be used"               , 0 },
  {"nprocesses",          N_PROCESSES_KEY          , "LPS"     ,  0                  ,  "Number of simulation objects"               , 0 },
  {"wall-timeout",        WALLCLOCK_TIMEOUT_KEY    , "SECONDS" ,  0                  ,  "End the simulation after SECONDS elapsed"   , 0 },
  {"observe-period",      OBS_PERIOD_KEY           , "MS"      ,  0                  ,  "Period in ms to check througput"    , 0 },
  {"disable-committer-threads",  DISABLE_COMMITTER_KEY, 0         ,  OPTION_ARG_OPTIONAL,  "Disable committer threads"   , 0 },
  
  {"ckpt-period",         CKPT_PERIOD_KEY          , "#EVENTS" ,  0                  ,  "Number of events to be forward-executed before taking a full-snapshot"   , 0 },
  {"ckpt-fossil-period",  CKPT_FOSSIL_PERIOD_KEY   , "#EVENTS" ,  0                  ,  "Number of events to be executed before collection committed snapshot"   , 0 },
  {"ckpt-autonomic-period",  CKPT_AUTONOMIC_PERIOD_KEY, 0         ,  OPTION_ARG_OPTIONAL,  "Enable autonomic checkpointing period"   , 0 },
  {"ckpt_forced_full_period",  CKPT_FORCED_FULL_PERIOD_KEY, "#CHECKPOINTS"         ,  0,  "Number of incremental checkpoints before taking a full log"   , 0 },
  {"ckpt-autoperiod-bound",  CKPT_AUTOPERIOD_BOUND_KEY, "#EVENTS",0                  ,  "Maximum value for autonomic checkpointing period"   , 0 },
  {"iss_enabled",               ISS_ENABLED ,         0        ,  OPTION_ARG_OPTIONAL,  "Use incremental state saving as checkpointing mechanism"   , 0 },
  {"iss_enabled_mprotection",   ISS_ENABLED_MPROTECTION ,         0        ,  OPTION_ARG_OPTIONAL,  "Use write protection as tracking mechanism for write accesses"   , 0 },
  {"iss_signal_mprotect",   ISS_SIGNAL_MPROTECT ,         0        ,  OPTION_ARG_OPTIONAL,  "Use mprotect() as tracking mechanism for write accesses"   , 0 },
  
  {"distributed-fetch",   DISTRIBUTED_FETCH_KEY    , 0         ,  OPTION_ARG_OPTIONAL,  "Enable distributed fetch"   , 0 },
  {"df-bound",            DISTRIBUTED_FETCH_BOUND_KEY, "#LPS"  ,  0                  ,  "Distributed fetch bound"   , 0 },
 
  {"numa-rebalance"   ,   NUMA_REBALANCE_KEY       , 0         ,  OPTION_ARG_OPTIONAL,  "Enable numa load-balancing"   , 0 },
  {"enable-mbind"       , ENABLE_MBIND_KEY         , 0         ,  OPTION_ARG_OPTIONAL,  "Enable mbind"   , 0 },
  {"enable-custom-alloc", ENABLE_CUSTOM_ALLOC_KEY  , 0         ,  OPTION_ARG_OPTIONAL,  "Enable custom alloc"   , 0 },
  {"segment-size-shift",  SEGMENT_SCALE_KEY        , "#SHIFTS" ,  0                  ,  "Segment size = (2**30)B >> #SHIFTS"   , 0 },

  {"ongvt-mode",          ONGVT_MODE_KEY           , "MODE-ID" ,  0                  ,  "0=Opportunistic (default) 1=Event-based period 2=millisecond-based period "   , 0 },
  {"ongvt-period",        ONGVT_PERIOD_KEY         , "PERIOD LEN" ,  0               ,  "Number of events/milliseconds to be executed before onGVT must be invoked"   , 0 },

  {"enforce-locality",    ENFORCE_LOCALITY_KEY     , 0         ,  OPTION_ARG_OPTIONAL,  "Use pipes to improve cache exploitation"    , 0 },
  {"el-dyn-window",       EL_DYN_WINDOW_KEY        , 0         ,  OPTION_ARG_OPTIONAL,  "Use pipes with dynamic window"    , 0 },
  {"el-window-size",      EL_WINDOW_SIZE_KEY       , "VTIME"   ,  0                  ,  "Sets the window size to be used with pipes"    , 0 },
  {"el-locked-size",      EL_LOCKED_PIPE_SIZE_KEY  , "COUNT"   ,  0                  ,  "Sets the locked pipe size"    , 0 },
  {"el-evicted-size",     EL_EVICTED_PIPE_SIZE_KEY , "COUNT"   ,  0                  ,  "Sets the evicted pipe size"    , 0 },
  {"el-th-trigger",       EL_TH_THRESHOLD_KEY      , "PERC"    ,  0                  ,  "Threshold to trigger a window resize"    , 0 },
  {"el-roll-th-trigger",  EL_WAIT_ROLLBACK_TH_KEY  , "PERC"    ,  0                  ,  "Rollback Threshold to trigger a window resize"    , 0 },
  {"el-th-trigger-counts",EL_TH_THRESHOLD_CNT_KEY  , "#COUNTS" ,  0                  ,  "Number of througput violations to trigger a window reset (default 0)"    , 0 },
  
  {"verbose",            'v'                       ,  0        ,  0                  ,  "Provide verbose output"                     , 0 },
  { 0, 0, 0, 0, 0, 0} 
};


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  // Figure out which option we are parsing, and decide
  // how to store it  
  switch (key)
    {
    case N_CORES_KEY:
      pdes_config.ncores = atoi(arg);
      pdes_config.serial = pdes_config.ncores == 1;
      break;
    case N_PROCESSES_KEY:
      pdes_config.nprocesses = atoi(arg);
      break;
    case WALLCLOCK_TIMEOUT_KEY:
      pdes_config.timeout = atoi(arg);
      break;
    case OBS_PERIOD_KEY:
      pdes_config.observe_period = atoi(arg);
      break;
    case DISABLE_COMMITTER_KEY:
      pdes_config.enable_committer_threads = 0;
      break;

    case CKPT_PERIOD_KEY:
      pdes_config.ckpt_period = atoi(arg);
      break;

    case CKPT_AUTONOMIC_PERIOD_KEY:
      pdes_config.ckpt_autonomic_period = 1;
      break;

    case ISS_ENABLED:
      pdes_config.checkpointing = INCREMENTAL_STATE_SAVING;
      break;

    case ISS_ENABLED_MPROTECTION:
      pdes_config.iss_enabled_mprotection = 1;
      break;

    case ISS_SIGNAL_MPROTECT:
      pdes_config.iss_signal_mprotect     = 1;

    case CKPT_FORCED_FULL_PERIOD_KEY:
      pdes_config.ckpt_forced_full_period = atoi(arg);
    break;

    case CKPT_FOSSIL_PERIOD_KEY:
      pdes_config.ckpt_collection_period = atoi(arg);
      break;
    case CKPT_AUTOPERIOD_BOUND_KEY:
      pdes_config.ckpt_autoperiod_bound = atoi(arg);
      break;


    case ONGVT_PERIOD_KEY:
      pdes_config.ongvt_period = atoi(arg);
      break;
    case ONGVT_MODE_KEY:
      pdes_config.ongvt_mode = atoi(arg);
      break;

    case DISTRIBUTED_FETCH_KEY:
      pdes_config.distributed_fetch = 1;
      pdes_config.df_bound = -1;
      break;
    case DISTRIBUTED_FETCH_BOUND_KEY:
      pdes_config.df_bound = atoi(arg);
      break;

    case NUMA_REBALANCE_KEY:
      pdes_config.numa_rebalance = 1;
      break;
    case ENABLE_MBIND_KEY:
      pdes_config.enable_mbind = 1;
      break;

    case ENABLE_CUSTOM_ALLOC_KEY:
      pdes_config.enable_custom_alloc = 1;
      break;

    case SEGMENT_SCALE_KEY:
      pdes_config.segment_shift = atoi(arg);
      break;
      

    case ENFORCE_LOCALITY_KEY:
      pdes_config.enforce_locality = 1;
      pdes_config.el_locked_pipe_size = 1;
      pdes_config.el_evicted_pipe_size = 1;
      pdes_config.el_window_size = 0;
      pdes_config.el_th_trigger = 0.025;
      break;
    
    case EL_DYN_WINDOW_KEY:
      pdes_config.el_dynamic_window = 1;
      break;
    case EL_WINDOW_SIZE_KEY:
      pdes_config.el_window_size = strtod(arg, NULL);
      break;
    case EL_LOCKED_PIPE_SIZE_KEY:
      pdes_config.el_locked_pipe_size = atoi(arg);
      break;
    case EL_EVICTED_PIPE_SIZE_KEY:
      pdes_config.el_evicted_pipe_size = atoi(arg);
      break;
    case EL_TH_THRESHOLD_KEY:
      pdes_config.el_th_trigger = strtod(arg, NULL);
      break;
    case EL_WAIT_ROLLBACK_TH_KEY:
      pdes_config.el_roll_th_trigger = strtod(arg, NULL);
      break;
    case EL_TH_THRESHOLD_CNT_KEY:
      pdes_config.th_below_threashold_cnt = atoi(arg);
      break;

    case ARGP_KEY_END:
      if(pdes_config.ckpt_period < 1 || pdes_config.ckpt_collection_period < 1 || pdes_config.ckpt_forced_full_period < 1){
        printf("Please set a non-zero checkpoint period\n");
        argp_usage (state);  
      }
      if(pdes_config.el_dynamic_window && !pdes_config.enforce_locality){
        printf("Please enable enforce-locality to use dynamic window\n");
        argp_usage (state);
      }
      if(pdes_config.el_locked_pipe_size && !pdes_config.enforce_locality){
        printf("Please enable enforce-locality to set locked pipe size\n");
        argp_usage (state);
      }
      if(pdes_config.el_evicted_pipe_size && !pdes_config.enforce_locality){
        printf("Please enable enforce-locality to set evicted pipe size\n");
        argp_usage (state);
      }
      if(!pdes_config.el_locked_pipe_size && pdes_config.enforce_locality){
        printf("Please set locked pipe size to enable enforce-locality\n");
        argp_usage (state);
      }
      if(!pdes_config.el_evicted_pipe_size && pdes_config.enforce_locality){
        printf("Please set evicted pipe size to enable enforce-locality\n");
        argp_usage (state);
      }
      if(pdes_config.el_window_size != 0.0 && !pdes_config.enforce_locality){
        printf("Please enable enforce-locality to set starting window size\n");
        argp_usage (state);
      }
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
      if(pdes_config.timeout < 0)
      {
        printf("Please provide a valid number of seconds which the simulation should last\n");
        argp_usage (state);
      }
      if(pdes_config.serial && pdes_config.ongvt_mode == MS_PERIODIC_ONGVT)
        pdes_config.ongvt_mode = 0; 
      break;
    
    default:
      return 0;
    
    }
  return 0;
}


void configuration_init(void){
  pdes_config.checkpointing = PERIODIC_STATE_SAVING;
  pdes_config.ckpt_period = 20;
  pdes_config.ckpt_collection_period = 100;
  pdes_config.ckpt_autonomic_period = 0;
  pdes_config.ckpt_forced_full_period = 1;
  pdes_config.ckpt_autoperiod_bound = -1;
  pdes_config.iss_enabled_mprotection = 0;
  pdes_config.iss_signal_mprotect = 0; //default is zero

  pdes_config.serial = false;
  pdes_config.observe_period = 500;

  pdes_config.timeout = 0;
  
  pdes_config.ongvt_period = 0;
  pdes_config.ongvt_mode = 0;

  pdes_config.enforce_locality = 0;
  pdes_config.el_roll_th_trigger = -1;
  pdes_config.th_below_threashold_cnt = 0; 

  pdes_config.distributed_fetch = 0;
  pdes_config.numa_rebalance = 0;
  pdes_config.enable_mbind = 0;

  pdes_config.enable_custom_alloc = 0;
  pdes_config.segment_shift = 4;
  pdes_config.enable_committer_threads = 1;
}

void print_config(void){
      printf(COLOR_CYAN "\nStarting an execution with %u THREADs, %u LPs :\n", pdes_config.ncores, pdes_config.nprocesses);
//#if SPERIMENTAL == 1
//    printf("\t- SPERIMENTAL features enabled.\n");
//#endif
//#if PREEMPTIVE == 1
//    printf("\t- PREEMPTIVE event realease enabled.\n");
//#endif
#if DEBUG == 1
    printf("\t- DEBUG mode enabled.\n");
#endif
    printf("\t- DYMELOR enabled.\n");
    printf("\t- CACHELINESIZE %u\n", CACHE_LINE_SIZE);
    printf("\t- OBSERVATION PERIOD %u\n", pdes_config.observe_period);
    printf("\t- CHECKPOINT\n");
    printf("\t\t|- period %u\n", pdes_config.ckpt_period);
    printf("\t\t|- autonomic %u\n", pdes_config.ckpt_autonomic_period);
    printf("\t\t|- bound %u\n", pdes_config.ckpt_autoperiod_bound);
    printf("\t\t|- collection %u\n", pdes_config.ckpt_collection_period);
    printf("\t\t|- ckpt mode %u\n", pdes_config.checkpointing);
    printf("\t\t\t|- incremental with mprotect %u\n", pdes_config.iss_enabled_mprotection);
    printf("\t- ON_GVT MODE %u\n", pdes_config.ongvt_mode);
    printf("\t- ON_GVT PERIOD %u\n", pdes_config.ongvt_period);
    printf("\t- ENFORCE_LOCALITY %u\n", pdes_config.enforce_locality);
    if(pdes_config.enforce_locality){
      printf("\t\t|- Starting window %f\n", pdes_config.el_window_size);
      printf("\t\t|- Dynamic  window %u\n", pdes_config.el_dynamic_window);
      printf("\t\t|- Througput drift for window trigger %f\n", pdes_config.el_th_trigger);
      printf("\t\t|- Rollback threshold for window trigger %f\n", pdes_config.el_roll_th_trigger);
    }
    printf("\t- DISTRIBUTED FETCH %u\n", pdes_config.distributed_fetch);
      printf("\t\t|- fetch bound %d\n", pdes_config.df_bound);
    
    printf("\t- MALLOC-BASED RECOVERABLE ALLOC %u\n", !pdes_config.enable_custom_alloc);
    if(!pdes_config.enable_custom_alloc)
      printf("\t\t|- SEGMENT SHIFT  %u\n", !pdes_config.segment_shift);
    printf("\t- ENABLED_COMMITTER THREADS %u\n", pdes_config.enable_committer_threads);
    printf("\t- NUMA REBALANCE %u\n",    pdes_config.numa_rebalance);
    printf("\t- MBIND %u\n",    pdes_config.enable_mbind);

    if(pdes_config.ongvt_mode == MS_PERIODIC_ONGVT)
    #if CSR_CONTEXT == 0
        printf("\t- CSR ASYNCH disabled.\n");
    #endif
    #if CSR_CONTEXT == 1
        printf("\t- CSR ASYNCH enabled.\n");
    #endif
    #if BUDDY == 1
        printf("\t- BUDDY SCHEME enabled.\n");
    #else
        printf("\t- BUDDY SCHEME disabled.\n");
    #endif
#if REPORT == 1
    printf("\t- REPORT prints enabled.\n");
#endif
//#if REVERSIBLE == 1
//    printf("\t- SPECULATIVE SIMULATION\n");
//#else
//    printf("\t- CONSERVATIVE SIMULATION\n");
//#endif
    printf("\n" COLOR_RESET);


}

extern struct argp_option model_options[];
extern char *model_name;
extern error_t model_parse_opt(int key, char *arg, struct argp_state *state);

static struct argp model_argp = { model_options, model_parse_opt, NULL, NULL, 0, 0, 0 };
static struct argp_child param_child[] = {
{&model_argp, 0, "Model specific", 0},
{NULL, 0, NULL, 0}
};

static struct argp argp       = { options,       parse_opt, args_doc, doc, &param_child[0], 0, 0 };

void parse_options(int argn, char **argv){
  configuration_init();
  argp_parse(&argp, argn, argv, 0, NULL, NULL);
}