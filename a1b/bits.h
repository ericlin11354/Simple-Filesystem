/**
 * CSC369 Assignment 1 - functions for bit operations.
 */
#pragma once

#include <errno.h>

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

/**
 * get the pointer to a data block
 *
 * @param img  		ptr to file img start
 * @param block_n  	offset in number of blocks relative to the first data block
 * @return       ptr to the start of data block 'block_n'
 */
void* get_dblock_address(void *img, unsigned int block_n)
{
	a1fs_superblock* superblock = img;
	return (void*)( (char*)img + (block_n + superblock->s_first_data_block)*A1FS_BLOCK_SIZE );
}

/**
 * get the pointer to an inode
 *
 * @param img  		ptr to file img start
 * @param inode_n  	offset in number of inodes relative to the root inode
 * @return       ptr to the start of inode 'inode_n'
 */
a1fs_inode* get_inode_address(void *img, unsigned int inode_n)
{
	a1fs_superblock* superblock = img;
	void *i_table = (char*)img + superblock->s_inode_table * A1FS_BLOCK_SIZE;
	a1fs_inode* root_inode = (a1fs_inode*)i_table;
	return (a1fs_inode*)(root_inode + inode_n);
}

/**
 * get the inode of the directory
 *
 * @param img  				ptr to file img start
 * @param inode_ptr_ptr  	ptr to inode ptr; inode ptr will be changed on success
 * @param block_n  			the path whose inode you're looking for
 * @return       ptr to the inode; -1 on error
 */
int get_inode_from_path(void *img, a1fs_inode **inode_ptr_ptr, const char *path)
{
	char path_cpy[strlen(path) + 1];
	strcpy(path_cpy, path);

	// a1fs_superblock* superblock = img;
	a1fs_inode* root_inode = get_inode_address(img, 0);
	a1fs_inode* curr_inode = root_inode;

    char *nextDir;
    nextDir = strtok(path_cpy, "/");

    while (nextDir != NULL)
    {
		int num_dirs = root_inode->size/sizeof(a1fs_dentry);
		int dirs_checked = 0;
		bool dir_found = false;
		
		//TODO: add support for multi extent directories
		while (dirs_checked < num_dirs){
			a1fs_dentry *dentries = get_dblock_address(img, curr_inode->i_block[1].start);
			a1fs_dentry *curr_dentry = &dentries[dirs_checked];
			if(strcmp(curr_dentry->name, nextDir) == 0){
				curr_inode = root_inode + curr_dentry->ino;
				nextDir = strtok (NULL, "/");

				dir_found = true;
				break;
			}

			++dirs_checked;
		}

		if (dir_found) continue;

		return -ENOENT;
    }

	*inode_ptr_ptr = curr_inode;
	return 0;
}