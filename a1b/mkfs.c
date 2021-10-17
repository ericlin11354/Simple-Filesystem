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
	(void)image;
	struct a1fs_superblock *sb = (struct a1fs_superblock *)(image + A1FS_BLOCK_SIZE);
	if (sb->magic == A1FS_MAGIC)
		return true;
	return false;
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
	(void)image;
	(void)size;
	(void)opts;
	struct a1fs_superblock *sb = (struct a1fs_superblock *)(image + A1FS_BLOCK_SIZE);
	sb->size = size;
	sb->magic = A1FS_MAGIC;

	sb->s_inodes_count = opts->n_inodes;
	sb->s_blocks_count = size / A1FS_BLOCK_SIZE;
	if (sb->s_blocks_count < 5) {
		fprintf(stderr, "File system too small");
		return false;
	}

	sb->s_inode_size = 128;
	unsigned int temp = opts->n_inodes % (size / sb->s_inode_size) != 0;
	unsigned int inode_table_size = ((opts->n_inodes / 
									(size / sb->s_inode_size)) + temp) * A1FS_BLOCK_SIZE;  

	sb->s_r_blocks_count = 0;

	sb->s_free_blocks_count = (size - A1FS_BLOCK_SIZE * 3 - inode_table_size) / A1FS_BLOCK_SIZE;
	sb->s_free_inodes_count = opts->n_inodes;

	sb->s_first_data_block = 4 + inode_table_size;

	// NOT SURE
	sb->s_first_ino = 0 ;

	sb->s_block_bitmap = 3;
	sb->s_inode_bitmap = 2;
	sb->s_inode_table =  4;

	struct stat st = {0}; // check if directory exists
	if (stat(opts->img_path, &st) == -1 ) 
		mkdir(opts->img_path, S_IFDIR | 0777);


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
