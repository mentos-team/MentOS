///                MentOS, The Mentoring Operating system project
/// @file initrd.c
/// @brief Headers of functions for initrd filesystem.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stdio.h"
#include "vfs.h"
#include "errno.h"
#include "debug.h"
#include "shell.h"
#include "kheap.h"
#include "fcntl.h"
#include "libgen.h"
#include "bitops.h"
#include "initrd.h"
#include "string.h"

char *module_start[MAX_MODULES];

initrd_t *fs_specs;
initrd_file_t *fs_headers;
unsigned int fs_end;

/// The list of file descriptors.
initrd_fd ird_descriptors[MAX_INITRD_DESCRIPTORS];

/// The currently opened file descriptor.
unsigned int cur_irdfd;

static inline initrd_file_t *get_initrd_file(const char *path)
{
	for (uint32_t i = 0; i < MAX_FILES; ++i) {
		// Discard the headers which has a different name.
		if (strcmp(path, fs_headers[i].fileName) == 0) {
			return &(fs_headers[i]);
		}
	}

	return NULL;
}

static inline size_t get_free_header(initrd_file_t **free_header)
{
	for (size_t i = 0; i < MAX_FILES; ++i) {
		// TODO: If the header has type 0, then I suppose it is not used.
		if (fs_headers[i].file_type == 0) {
			(*free_header) = &(fs_headers[i]);
			return i;
		}
	}

	return 0;
}

uint32_t initfs_init()
{
	fs_end = 0;
	fs_specs = (initrd_t *)module_start[0];
	fs_headers = (initrd_file_t *)(module_start[0] + sizeof(initrd_t));

	for (int i = 0; i < MAX_INITRD_DESCRIPTORS; ++i) {
		ird_descriptors[i].file_descriptor = -1;
		ird_descriptors[i].cur_pos = 0;
	}
	cur_irdfd = 0;
	printf(" * Number of Files: %d\n", fs_specs->nfiles);
	fs_end = fs_headers[(fs_specs->nfiles) - 1].offset +
			 fs_headers[(fs_specs->nfiles) - 1].length;
	printf(" * Filesystem end : %d\n", fs_end);
	//dump_initrd_fs();

	return fs_specs->nfiles;
}

DIR *initfs_opendir(const char *path)
{
	initrd_file_t *direntry = get_initrd_file(path);

	if ((direntry == NULL) && (strcmp(path, "/") != 0)) {
		dbg_print("Cannot find '%s'\n", path);

		return NULL;
	}

	DIR *pdir = kmalloc(sizeof(DIR));
	pdir->handle = -1;
	pdir->cur_entry = 0;
	strcpy(pdir->path, path);

	return pdir;
}

int initfs_closedir(DIR *dirp)
{
	if (dirp != NULL) {
		kfree(dirp);
	}

	return 0;
}

dirent_t *initrd_readdir(DIR *dirp)
{
	if (dirp->cur_entry >= MAX_FILES) {
		return NULL;
	}

	for (; dirp->cur_entry < MAX_FILES; ++dirp->cur_entry) {
		initrd_file_t *entry = &fs_headers[dirp->cur_entry];
		if (entry->fileName[0] == '\0') {
			continue;
		}
		// Get the directory of the file.
		char *filedir = dirname(entry->fileName);

		// Check if directory path and file directory are the same, or if
		// the directory is the root and the file directory is dot.
		if (strcmp(dirp->path, filedir) == 0) {
			dirp->entry.d_ino = dirp->cur_entry;
			dirp->entry.d_type = entry->file_type;
			strcpy(dirp->entry.d_name, entry->fileName);
			++dirp->cur_entry;

			return &(dirp->entry);
		}
	}

	return NULL;
}

int initrd_mkdir(const char *path, mode_t mode)
{
	(void)mode;
	initrd_file_t *direntry = NULL;

	// Check if the directory already exists.
	direntry = get_initrd_file(path);
	if (direntry != NULL) {
		printf("initrd_mkdir: cannot create directory '%s': "
			   "File exists\n\n",
			   path);
		return -1;
	}
	// Check if the directories before it exist.
	if ((strcmp(dirname(path), ".") != 0) &&
		(strcmp(dirname(path), "/") != 0)) {
		dbg_print("initrd_mkdir: %s\n", dirname(path));
		direntry = get_initrd_file(dirname(path));
		if (direntry == NULL) {
			printf("initrd_mkdir: cannot create directory '%s': "
				   "No such file or directory\n\n",
				   path);

			return -1;
		}
		if ((direntry->file_type != FS_DIRECTORY) &&
			(direntry->file_type != FS_MOUNTPOINT)) {
			printf("initrd_mkdir: cannot create directory '%s': "
				   "Not a directory\n\n",
				   path);

			return -1;
		}
	}
	// Get a free header.
	initrd_file_t *free_header = NULL;
	get_free_header(&free_header);
	if (free_header == NULL) {
		printf("initrd_mkdir: cannot create directory '%s': "
			   "Maximum number of headers reached\n\n",
			   path);

		return -1;
	}
	// Create the directory.
	free_header->magic = 0xBF;
	strcpy(free_header->fileName, path);
	free_header->file_type = FS_DIRECTORY;
	free_header->uid = current_user.uid;
	free_header->offset = ++fs_end;
	free_header->length = 0;
	// Increase the number of files.
	++fs_specs->nfiles;

	return 1;
}

int initrd_rmdir(const char *path)
{
	initrd_file_t *direntry = NULL;

	// Check if the directory exists.
	direntry = get_initrd_file(path);
	if (direntry == NULL) {
		//errno = ENOENT;
		return -1;
	}

	if ((direntry->file_type != FS_DIRECTORY)) {
		//errno = ENOTDIR;
		return -1;
	}

	for (int i = 0; i < MAX_FILES; ++i) {
		initrd_file_t *entry = &fs_headers[i];
		if (entry->fileName[0] == '\0') {
			continue;
		}
		// Get the directory of the file.
		char *filedir = dirname(entry->fileName);
		// Check if directory path and file directory are the same.
		if (strcmp(direntry->fileName, filedir) == 0) {
			//errno = ENOTEMPTY;
			return -1;
		}
	}
	// Remove the directory.
	direntry->magic = 0;
	memset(direntry->fileName, 0, MAX_FILENAME_LENGTH);
	direntry->file_type = 0;
	direntry->uid = 0;
	direntry->offset = 0;
	direntry->length = 0;
	// Decrease the number of files.
	--fs_specs->nfiles;

	return 0;
}

int initfs_open(const char *path, int flags, ...)
{
	// If we have reached the maximum number of descriptors, just try to find
	// a non-used one.
	if (cur_irdfd >= MAX_INITRD_DESCRIPTORS) {
		// Reset the current ird file descriptor.
		cur_irdfd = 0;
		while ((ird_descriptors[cur_irdfd].file_descriptor != -1) &&
			   (cur_irdfd < MAX_INITRD_DESCRIPTORS)) {
			++cur_irdfd;
		}
	}

	for (uint32_t it = 0; it < fs_specs->nfiles; ++it) {
		// Discard the headers which has a different name.
		if (strcmp(path, fs_headers[it].fileName) != 0) {
			continue;
		}

		// However, if the name is the same, but the file type is different,
		// stop the function and return failure value.
		if ((fs_headers[it].file_type == FS_DIRECTORY) ||
			(fs_headers[it].file_type == FS_MOUNTPOINT)) {
			//errno = EISDIR;
			return -1;
		}

		if (has_flag(flags, O_CREAT)) {
			//errno = EEXIST;
			return -1;
		}

		// Otherwise, if the file is correct, update
		ird_descriptors[cur_irdfd].file_descriptor = it;
		ird_descriptors[cur_irdfd].cur_pos = 0;
		if (has_flag(flags, O_APPEND)) {
			ird_descriptors[cur_irdfd].cur_pos = fs_headers[it].length;
		}

		return cur_irdfd++;
	}
	if (has_flag(flags, O_CREAT)) {
		// Check if the directories before it exist.
		if ((strcmp(dirname(path), ".") != 0) &&
			(strcmp(dirname(path), "/") != 0)) {
			initrd_file_t *direntry = get_initrd_file(dirname(path));
			if (direntry == NULL) {
				//errno = ENOENT;
				return -1;
			}
		}
		// Get a free header.
		initrd_file_t *free_header = NULL;
		size_t fd = get_free_header(&free_header);
		if (free_header == NULL) {
			//errno = ENFILE;
			return -1;
		}
		// Create the file.
		free_header->magic = 0xBF;
		strcpy(free_header->fileName, path);
		free_header->file_type = FS_FILE;
		free_header->uid = current_user.uid;
		free_header->offset = ++fs_end;
		free_header->length = 0;
		// Set the descriptor.
		ird_descriptors[cur_irdfd].file_descriptor = fd;
		ird_descriptors[cur_irdfd].cur_pos = 0;
		// Increase the number of files.
		++fs_specs->nfiles;

		return cur_irdfd++;
	}

	//errno = ENOENT;
	return -1;
}

int initfs_remove(const char *path)
{
	initrd_file_t *file = get_initrd_file(path);

	if (file == NULL) {
		return -1;
	}

	if (file->file_type != FS_FILE) {
		return -1;
	}

	// Remove the directory.
	file->magic = 0;
	memset(file->fileName, 0, MAX_FILENAME_LENGTH);
	file->file_type = 0;
	file->uid = 0;
	file->offset = 0;
	file->length = 0;
	// Decrease the number of files.
	--fs_specs->nfiles;

	return 0;
}

ssize_t initfs_read(int fildes, char *buf, size_t nbyte)
{
	// If the number of byte to read is zero, skip.
	if (nbyte == 0) {
		return 0;
	}

	// Get the file descriptor of the file.
	int lfd = ird_descriptors[fildes].file_descriptor;

	// Get the current position.
	int read_pos = ird_descriptors[fildes].cur_pos;

	// Get the legnth of the file.
	int file_size = fs_headers[lfd].length;

	// Get the begin of the file.
	char *file_start = (module_start[0] + fs_headers[lfd].offset);

	// Declare an iterator.
	size_t it = 0;

	while ((it < nbyte) && (read_pos < file_size)) {
		*buf++ = file_start[read_pos];
		++read_pos;
		++it;
	}

	ird_descriptors[fildes].cur_pos = read_pos;
	if (read_pos == file_size) {
		return EOF;
	}

	return nbyte;
}

int initrd_stat(const char *path, stat_t *stat)
{
	int i;
	i = 0;

	while (i < MAX_FILES) {
		if (!strcmp(path, fs_headers[i].fileName)) {
			stat->st_uid = fs_headers[i].uid;
			stat->st_size = fs_headers[i].length;

			break;
		}
		i++;
	}
	//dbg_print("Initrd stat function\n");
	//buf->st_uid = 33;
	if (i == MAX_FILES) {
		return -1;
	} else {
		return 0;
	}
}

ssize_t initrd_write(int fildes, const void *buf, size_t nbyte)
{
	// If the number of byte to write is zero, skip.
	if (nbyte == 0) {
		return 0;
	}

	// Make a copy of the buffer.
	char *tmp = (char *)kmalloc(strlen(buf) * sizeof(char));
	strcpy(tmp, buf);

	// Get the file descriptor of the file.
	int lfd = ird_descriptors[fildes].file_descriptor;

	printf("Please wait, im writing the world...\n");
	printf("And the world begun with those words: %s and his mark his: %d\n",
		   tmp, lfd);
	// Get the begin of the file.
	char *file_start = (module_start[0] + fs_headers[lfd].offset +
						ird_descriptors[fildes].cur_pos);
	// Declare an iterator.
	size_t it = 0;
	while (it <= nbyte) {
		file_start[it] = tmp[it];
		++it;
	}

	// Increment the length of the file.
	fs_headers[lfd].length = fs_headers[lfd].length + it;
	// Free the memory of the temporary file.
	kfree(tmp);
	// Return the number of written bytes.

	return it;
}

int initrd_close(int fildes)
{
	ird_descriptors[fildes].file_descriptor = -1;
	ird_descriptors[fildes].cur_pos = 0;

	return 0;
}

size_t initrd_nfiles()
{
	size_t nfiles = 0;

	for (size_t i = 0; i < MAX_FILES; ++i) {
		if (fs_headers[i].file_type != 0) {
			++nfiles;
		}
	}

	return nfiles;
}

void dump_initrd_fs()
{
	for (uint32_t i = 0; i < MAX_FILES; ++i) {
		dbg_print("[%2d] %s\n", i, fs_headers[i].fileName);
	}
}
