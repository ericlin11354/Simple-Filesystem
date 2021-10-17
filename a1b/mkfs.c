/*
 * This code is provided solely for the personal and private use of students
 * taking the CSC369H course at the University of Toronto. Copying for purposes
 * other than this use is expressly prohibited. All forms of distribution of
 * this code, including but not limited to public repositories on GitHub,
 * GitLab, Bitbucket, or any other online platform, whether as given or with
 * any changes, are expressly prohibited.
 *
 * Authors: Alexey Khrabrov, Karen Reid
 *
 * All of the files in this directory and all subdirectories are:
 * Copyright (c) 2019, 2021 Karen Reid
 */

/**
 * CSC369 Assignment 1 - a1fs formatting tool.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#include "a1fs.h"
#include "map.h"


/** Command line options. */
typedef struct mkfs_opts {
	/** File system image file path. */
	const char *img_path;
	/** Number of inodes. */
	size_t n_inodes;

	/** Print help and exit. */
	bool help;
	/** Overwrite existing file system. */
	bool force;
	/** Zero out image contents. */
	bool zero;

} mkfs_opts;

static const char *help_str = "\
Usage: %s options image\n\
\n\
Format the image file into a1fs file system. The file must exist and\n\
its size must be a multiple of a1fs block size - %zu bytes.\n\
\n\
Options:\n\
    -i num  number of inodes; required argument\n\
    -h      print help and exit\n\
    -f      force format - overwrite existing a1fs file system\n\
    -z      zero out image contents\n\
";

static void print_help(FILE *f, const char *progname)
{
	fprintf(f, help_str, progname, A1FS_BLOCK_SIZE);
}


static bool parse_args(int argc, char *argv[], mkfs_opts *opts)
{
	char o;
	while ((o = getopt(argc, argv, "i:hfvz")) != -1) {
		switch (o) {
			case 'i': opts->n_inodes = strtoul(optarg, NULL, 10); break;

			case 'h': opts->help  = true; return true;// skip other arguments
			case 'f': opts->force = true; break;
			case 'z': opts->zero  = true; break;

			case '?': return false;
			default : assert(false);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Missing image path\n");
		return false;
	}
	opts->img_path = argv[optind];

	if (!opts->n_inodes) {
		fprintf(stderr, "Missing or invalid number of inodes\n");
		return false;
	}
	return true;
}


/** Determine if the image has already been formatted into a1fs. */
static bool a1fs_is_present(void *image)
{
	//TODO: check if the image already contains a valid a1fs superblock
	return ((a1fs_superblock*)image)->magic == A1FS_MAGIC;
}


/**
 * Set a bit in a bitmap
 *
 * @param bitmap
 * @param bit_x
 * @param target
 */
static void set_bit(char *bitmap, unsigned int bit_x, unsigned int target)
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
 * Get the number of blocks an object should take up.
 *
 * @param obj_size  size of an object
 * @return       number of blocks the object should take up.
 */
static int get_block_size(unsigned int obj_size)
{
	return obj_size/A1FS_BLOCK_SIZE + (obj_size % A1FS_BLOCK_SIZE != 0);
}


/**
 * Format the image into a1fs.
 *
 * NOTE: Must update mtime of the root directory.
 *
 * @param image  pointer to the start of the image.
 * @param size   image size in bytes.
 * @param opts   command line options.
 * @return       true on success;
 *               false on error, e.g. options are invalid for given image size.
 */
static bool mkfs(void *image, size_t size, mkfs_opts *opts)
{
	//TODO: initialize the superblock and create an empty root directory
	//NOTE: the mode of the root directory inode should be set to S_IFDIR | 0777

	//init struct members
	a1fs_superblock* superblock = image;
	superblock->magic = A1FS_MAGIC;
	superblock->size = size;

	unsigned int total_blocks_count = size / A1FS_BLOCK_SIZE;
	if (total_blocks_count < 5){ printf("1"); return false; }
	
	unsigned int inode_table_bsize = get_block_size(opts->n_inodes * sizeof(a1fs_inode));
	unsigned int inode_bitmap_bsize = get_block_size(opts->n_inodes / 8 + (opts->n_inodes % 8 != 0));
	
	//data_bitmap_and_data_block_block_size
	unsigned int  dbitmap_and_dblock_bsize = 
		total_blocks_count - inode_table_bsize - inode_bitmap_bsize - 1;
	
	unsigned int data_bitmap_bsize = get_block_size(dbitmap_and_dblock_bsize / 8 + (dbitmap_and_dblock_bsize % 8 != 0));
	unsigned int data_bsize = dbitmap_and_dblock_bsize - data_bitmap_bsize;

	superblock->s_inode_bitmap = 1;
	superblock->s_block_bitmap = 1 + inode_bitmap_bsize;

	superblock->s_inode_table = 1 + inode_bitmap_bsize + data_bitmap_bsize;
	//set all bitmap to 0
	memset((char*)image + A1FS_BLOCK_SIZE, 0, (superblock->s_inode_table - 1)*A1FS_BLOCK_SIZE); 

	superblock->s_first_data_block = 1 + inode_bitmap_bsize + data_bitmap_bsize + inode_table_bsize;

	superblock->s_blocks_count = total_blocks_count;
	superblock->s_d_blocks_count = data_bsize;
	superblock->s_r_blocks_count = total_blocks_count - data_bsize;
	
	superblock->s_inodes_count = opts->n_inodes;
	superblock->s_inode_size = sizeof(a1fs_inode);
	superblock->s_first_ino = 1;

	//make root directory
	a1fs_inode* root_inode = (void*)((char*)image + A1FS_BLOCK_SIZE*(superblock->s_inode_table));
	root_inode->mode = S_IFDIR | 0777;
	clock_gettime(CLOCK_REALTIME, &(root_inode->mtime));

	superblock->s_free_inodes_count = opts->n_inodes - 1;
	set_bit((char*)image + superblock->s_inode_bitmap*A1FS_BLOCK_SIZE, 0, 1);

	//make directory block
	a1fs_dentry* root_dir = (void*)((char*)image + A1FS_BLOCK_SIZE*(superblock->s_first_data_block));
	root_dir[0].ino = 0;
	strcpy(root_dir[0].name, ".");
	root_dir[1].ino = 0;
	strcpy(root_dir[1].name, "..");
	root_inode->links = 2;
	root_inode->size = sizeof(a1fs_dentry) * 2;

	superblock->s_free_blocks_count = data_bsize - 1;
	set_bit((char*)image + superblock->s_block_bitmap*A1FS_BLOCK_SIZE, 0, 1);

	return true;
}


int main(int argc, char *argv[])
{
	mkfs_opts opts = {0};// defaults are all 0
	if (!parse_args(argc, argv, &opts)) {
		// Invalid arguments, print help to stderr
		print_help(stderr, argv[0]);
		return 1;
	}
	if (opts.help) {
		// Help requested, print it to stdout
		print_help(stdout, argv[0]);
		return 0;
	}

	// Map image file into memory
	size_t size;
	void *image = map_file(opts.img_path, A1FS_BLOCK_SIZE, &size);
	if (!image) {
		return 1;
	}

	// Check if overwriting existing file system
	int ret = 1;
	if (!opts.force && a1fs_is_present(image)) {
		fprintf(stderr, "Image already contains a1fs; use -f to overwrite\n");
		goto end;
	}

	if (opts.zero) {
		memset(image, 0, size);
	}
	if (!mkfs(image, size, &opts)) {
		fprintf(stderr, "Failed to format the image\n");
		goto end;
	}

	ret = 0;
end:
	munmap(image, size);
	return ret;
}
