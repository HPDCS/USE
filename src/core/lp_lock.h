#ifndef __LP_LOCK__
#define __LP_LOCK__

extern volatile unsigned int *lp_lock;


static inline unsigned int haveLock( unsigned int lp){ return (lp_lock[lp*CACHE_LINE_SIZE/4]) == (tid+1); }
static inline unsigned int checkLock(unsigned int lp){ return (lp_lock[lp*CACHE_LINE_SIZE/4]); }


#if STATE_SWAPPING == 1 && CSR_CONTEXT == 1
extern __thread volatile unsigned int potential_locked_object;
#endif

static inline unsigned int tryLock( unsigned int lp){
#if STATE_SWAPPING == 1 && CSR_CONTEXT == 1
    __sync_lock_test_and_set(&potential_locked_object, lp);
#endif
    return 
        (lp_lock[lp*CACHE_LINE_SIZE/4]==0) &&  
        (__sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], 0, tid+1));
}


static inline unsigned int unlock(unsigned int lp){ 
#if ENFORCE_LOCALITY == 1
  #if DEBUG == 1
    assertf(lp == UNDEFINED_LP, "trying to unlock an undefined LP%s", "\n");
    assertf(!haveLock(lp), "trying to unlock without own lock %s", "\n");
  #endif
  return LPS[lp]->wt_binding == tid ||          
#else    
  return 
#endif
        __sync_bool_compare_and_swap(&lp_lock[lp*CACHE_LINE_SIZE/4], tid+1, 0); 
}

#endif