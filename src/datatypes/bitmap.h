/**
 * 
 * @file bitmap.h
 * @brief This module implemets an arbitrary sized bitmap
 * @author Romolo Marotta
 * @date September 23, 2022
*/

#ifndef __BITMAP__
#define __BITMAP__

#include <limits.h>
#include <strings.h>
#include <stdlib.h>
#include <atomic.h>


/**
 * @brief A generic bitmap
 */
typedef struct __bitmap{
    unsigned int virtual_len; /// request bitmap length
    unsigned int actual_len;  /// multiple of CHAR_BIT
    unsigned char bits[];     /// array of chars storing the bitmap
} bitmap;


/**
 * @brief Allocate a bitmap
 * @param len: the number of bit contained within the bitmap
 * @return a pointer to a bitmap struct on success, otherwise returns null
 */ 
static inline bitmap* allocate_bitmap(unsigned int len){
    unsigned int bytes      ;
    unsigned int actual_len ;
    bitmap *tmp             ;


    bytes        =  len  / CHAR_BIT;
    actual_len   = bytes * CHAR_BIT;

    
    bytes       += actual_len<len;
    actual_len   = bytes * CHAR_BIT;

    tmp = (bitmap*) malloc(bytes+2*sizeof(unsigned int));
    
    bzero(tmp, bytes);
    
    tmp->virtual_len = len;
    tmp->actual_len  = actual_len;
    
    return tmp;
}

/**
 * @param ptr: the reference to a bitmap to be deallocated
 */ 
static inline void release_bitmap(bitmap* ptr){
    if(ptr == NULL ) abort();
    free(ptr);
}

/**
 * @brief It sets to 1 the bit at the given pos within a bitmap
 * @param ptr: the reference to a bitmap
 * @param pos: the position of the bit to be set
 */ 
static inline void set_bit(bitmap* ptr, unsigned int pos){
    if(ptr == NULL || ptr->actual_len <= pos ) abort();
    unsigned int  index = pos/CHAR_BIT;
    unsigned int  b_pos = pos%CHAR_BIT;
    unsigned char b_val = 1 << b_pos;
    ptr->bits[index] |= b_val; 
}

static inline void atomic_set_bit(bitmap* ptr, unsigned int pos){
    if(ptr == NULL || ptr->actual_len <= pos ) abort();
    unsigned int index, b_pos;
 //   unsigned char b_val, b_old;
//begin:
    index = pos/CHAR_BIT;
    b_pos = pos%CHAR_BIT;
//    b_old = ptr->bits[index];
//    b_val = b_old | (1 << b_pos);
    atomic_bit_test_and_set_x86(ptr->bits+index, b_pos);
//    if(!__sync_bool_compare_and_swap(ptr->bits+index, b_old, b_val)) goto begin;
}

/**
 * @brief sets to 0 the bit at the given pos within a bitmap
 * @param ptr: the reference to a bitmap
 * @param pos: the position of the bit to be set
 */ 
static inline void reset_bit(bitmap* ptr, unsigned int pos){
    if(ptr == NULL || ptr->actual_len <= pos ) abort();
    unsigned int  index = pos/CHAR_BIT;
    unsigned int  b_pos = pos%CHAR_BIT;
    unsigned char b_val = ~(1 << b_pos);
    ptr->bits[index] &= b_val; 
}

/**
 * @brief returns the bit at the given pos within a bitmap
 * @param ptr: the reference to a bitmap
 * @param pos: the position of the bit to be read
 */ 
static inline char get_bit(bitmap* ptr, unsigned int pos){
    if(ptr == NULL || ptr->actual_len <= pos ) abort();
    unsigned int  index = pos/CHAR_BIT;
    unsigned int  b_pos = pos%CHAR_BIT;
    unsigned char b_val = (1 << b_pos);
    return (ptr->bits[index] & b_val) > 0; 
}


static inline void clear_bitmap(bitmap *ptr) {
    if(ptr == NULL) abort();
    unsigned int index;
    for (index = 0; index < ptr->actual_len; index++) { 
        if (get_bit(ptr, index) != 0) {
            reset_bit(ptr, index);
        }
    }


}

/**
 * @brief assign the given value to the bit at the given pos within a bitmap
 * @param ptr: the reference to a bitmap
 * @param pos: the position of the bit to be written
 * @param pos: the value to be assigned
 */ 
static inline void assign_bit(bitmap* ptr, unsigned int pos, unsigned int val){
    if(val) set_bit(ptr,pos);
    else    reset_bit(ptr,pos);
}


#endif