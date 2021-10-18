/**
 * CSC369 Assignment 1 - functions for bit operations.
 */
#pragma once

#include "a1fs.h"

/**
 * Set a bit in a bitmap
 *
 * @param bitmap	bit_x is relative to this pointer
 * @param bit_x		position of desired bit
 * @param target	bit_x will be set to this; should be zero or one
 */
void set_bit(char *bitmap, unsigned int bit_x, unsigned int target)
{
	unsigned int byte_x = bit_x / 8;
	bit_x = bit_x % 8;

	if (target == 0){
		*((unsigned char *) (bitmap + byte_x)) &= ~(1 << bit_x);
	} else {
		*((unsigned char *) (bitmap + byte_x)) |= 1 << bit_x;
	}
}

/**
 * Get a bit in a bitmap
 *
 * @param bitmap	bit_x is relative to this pointer
 * @param bit_x		position of desired bit
 * @return          the value of bit_x; should be zero or one
 */
int get_bit(char *bitmap, unsigned int bit_x)
{
	unsigned int byte_x = bit_x / 8;
	bit_x = bit_x % 8;

	return (*((unsigned char *) (bitmap + byte_x)) >> bit_x) & 1;
}

/**
 * Get the number of blocks an object should take up.
 *
 * @param obj_size  size of an object in bytes
 * @return       number of blocks the object should take up.
 */
int get_block_size(unsigned int obj_size)
{
	return obj_size/A1FS_BLOCK_SIZE + (obj_size % A1FS_BLOCK_SIZE != 0);
}