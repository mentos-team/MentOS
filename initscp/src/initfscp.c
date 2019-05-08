/// @file   initfscp.c
/// @brief

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initfscp.h>
#include <stdbool.h>
#include <stdint.h>
#include <libgen.h>
#include <dirent.h>
#include <assert.h>

static FILE *target_fs = NULL;
static initrd_file_t headers[MAX_FILES];
static char mount_points[MAX_FILES][MAX_FILENAME_LENGTH];
static int mountpoint_idx = 0;
static int header_idx = 0;
static int header_offset = 0;

static inline void usage(char *prgname)
{
	printf("Usage:\n");
	printf(" %s --help        For this screen\n", prgname);
	printf(" %s --source [-s] The source directory.\n", prgname);
	printf(" %s --target [-t] The target file for the initfs.\n", prgname);
}

static inline void version(char *prgname)
{
	printf("%s version: %s\n", prgname, INITFSCP_VER);
}

static inline bool is_option_mount_point(char *arg)
{
	if (arg == NULL)
		return false;
	return ((strcmp(arg, "-m") == 0) || (strcmp(arg, "--mountpoint") == 0));
}

static inline bool is_option_source(char *arg)
{
	if (arg == NULL)
		return false;
	return ((strcmp(arg, "-s") == 0) || (strcmp(arg, "--source") == 0));
}

static inline bool is_option_target(char *arg)
{
	if (arg == NULL)
		return false;
	return ((strcmp(arg, "-t") == 0) || (strcmp(arg, "--target") == 0));
}

static inline bool is_mount_point(char *name)
{
	if (name == NULL)
		return false;
	// Check if the name matches with a mount-point.
	for (int i = 0; i < mountpoint_idx; ++i) {
		if (strcmp(name, mount_points[i]) == 0) {
			return true;
		}
	}
	return false;
}

static FILE *openfile(const char *dirname, struct dirent *dir, const char *mode)
{
	char pathname[1024];
	FILE *fp;
	sprintf(pathname, "%s/%s", dirname, dir->d_name);
	fp = fopen(pathname, mode);
	return fp;
}

static bool open_target_fs(int argc, char *argv[])
{
	printf("%-64s", "Opening target filesystem...");
	for (int i = 1; i < argc; ++i) {
		if (is_option_target(argv[i]) && ((i + 1) < argc)) {
			target_fs = fopen(argv[i + 1], "w");
			printf("[" GREEN "DONE" RESET "]\n\n");
			return true;
		}
	}
	printf("[" RED "FAILED" RESET "]\n\n");
	return false;
}

static bool init_headers()
{
	printf("%-64s", "Initializing headers structures...");
	for (size_t i = 0; i < MAX_FILES; i++) {
		headers[i].magic = 0xBF;
		memset(headers[i].fileName, 0, MAX_FILENAME_LENGTH);
		headers[i].file_type = 0;
		headers[i].uid = 0;
		headers[i].offset = 0;
		headers[i].length = 0;
	}
	printf("[" GREEN "DONE" RESET "]\n\n");
	return true;
}

static bool init_mount_points(int argc, char *argv[])
{
	printf("Initializing mount points...\n");
	for (int i = 1; i < argc; ++i) {
		if (is_option_mount_point(argv[i]) && ((i + 1) < argc)) {
			strcpy(mount_points[mountpoint_idx], argv[i + 1]);
			printf("[%3d] MPNT: %-52s", mountpoint_idx,
				   mount_points[mountpoint_idx]);
			printf("[" CYAN "DONE" RESET "]\n");
			++mountpoint_idx;
		}
	}
	printf("[" GREEN "DONE" RESET "]\n\n");
	return true;
}

static bool create_file_headers(char *mountpoint, char *directory)
{
	assert(mountpoint && "Received null mountpoint.");
	assert(directory && "Received null directory.");

	// ------------------------------------------------------------------------
	char absolute_path[256];
	memset(absolute_path, 0, 256);
	strcpy(absolute_path, mountpoint);
	strcat(absolute_path, directory);

	// ------------------------------------------------------------------------
	DIR *source_dir = opendir(absolute_path);
	if (source_dir == NULL) {
		printf("[" RED "FAILED" RESET "]\n");
		printf("Could not open source directory %s.\n", absolute_path);
		return false;
	}
	struct dirent *entry;
	while ((entry = readdir(source_dir)) != NULL) {
		if ((strcmp(entry->d_name, ".") == 0) ||
			(strcmp(entry->d_name, "..") == 0)) {
			continue;
		}
		if (entry->d_type != DT_DIR) {
			char relative_filename[256];
			strcpy(relative_filename, directory);
			strcat(relative_filename, "/");
			strcat(relative_filename, entry->d_name);

			// ----------------------------------------------------------------
			FILE *fd = openfile(absolute_path, entry, "r+");
			printf("[%3d] FILE: %-52s", header_idx, relative_filename);
			if (fd == NULL) {
				printf("[" RED "FAILED" RESET "]\n");
				printf("Error while opening file : %s\n", relative_filename);
			}
			fseek(fd, 0, SEEK_END);
			long length = ftell(fd);
			fclose(fd);

			// ----------------------------------------------------------------
			strcpy(headers[header_idx].fileName, relative_filename);
			headers[header_idx].file_type = FS_FILE;
			headers[header_idx].length = length;
			headers[header_idx].offset = header_offset;

			// ----------------------------------------------------------------
			printf("[" BLUE "OPEN" RESET "]");
			printf("[OFFSET:%6d]", headers[header_idx].offset);
			printf("[SIZE:%6d]", headers[header_idx].length);
			printf("[" GREEN "DONE" RESET "]\n");

			header_offset += headers[header_idx].length;
			++header_idx;
		} else {
			// ----------------------------------------------------------------
			char sub_directory[256];
			memset(sub_directory, 0, 256);
			strcpy(sub_directory, directory);
			strcat(sub_directory, "/");
			strcat(sub_directory, entry->d_name);

			// ----------------------------------------------------------------
			strcpy(headers[header_idx].fileName, sub_directory);
			headers[header_idx].file_type = FS_DIRECTORY;
			headers[header_idx].length = 0;
			headers[header_idx].offset = 0 /*++header_offset*/;

			// ----------------------------------------------------------------
			// Check if the directory is a mountpoint.
			if (is_mount_point(headers[header_idx].fileName)) {
				headers[header_idx].file_type = FS_MOUNTPOINT;
			}

			// ----------------------------------------------------------------
			printf("[%3d] %3s : %-52s", header_idx,
				   (headers[header_idx].file_type == FS_DIRECTORY) ? "DIR" :
																	 "MPT",
				   sub_directory);
			printf("[" BLUE "OPEN" RESET "]");
			printf("[OFFSET:%6d]", headers[header_idx].offset);
			printf("[SIZE:%6d]", headers[header_idx].length);
			printf("[" GREEN "DONE" RESET "]\n");

			// ----------------------------------------------------------------
			++header_idx;

			// ----------------------------------------------------------------
			create_file_headers(mountpoint, sub_directory);
		}
	}
	closedir(source_dir);
	return true;
}

static bool write_file_system(char *mountpoint)
{
	printf("Copying data to filesystem...\n");
	for (int i = 0; i < MAX_FILES; i++) {
		// --------------------------------------------------------------------
		char absolute_path[256];
		memset(absolute_path, 0, 256);
		strcpy(absolute_path, mountpoint);
		strcat(absolute_path, headers[i].fileName);

		// --------------------------------------------------------------------
		if (headers[i].file_type == FS_FILE) {
			FILE *fd2 = fopen(absolute_path, "r+");
			if (fd2 == NULL) {
				continue;
			}
			printf("[%3d] FILE: %-92s", i, absolute_path);
			char *buffer = (char *)malloc(headers[i].length);
			fread(buffer, 1, headers[i].length, fd2);
			fwrite(buffer, 1, headers[i].length, target_fs);
			printf("[" GREEN "DONE" RESET "]\n");
			fclose(fd2);
			free(buffer);
		}
	}
	printf("[" GREEN "DONE" RESET "]\n\n");
	return true;
}

int main(int argc, char *argv[])
{
	printf("Welcome to MentOS initfs file copier tool\n\n");
	if (argc <= 1) {
		if (argv[0][0] == '.') {
			version(argv[0] + 2);
		}
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")) {
		version(argv[0] + 2);
		return EXIT_SUCCESS;
	}
	if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	// ------------------------------------------------------------------------
	// Open the fs.
	if (!open_target_fs(argc, argv)) {
		printf("Could not open target FileSystem.\n");
		return EXIT_FAILURE;
	}

	// ------------------------------------------------------------------------
	// Initialize the headers.
	if (!init_headers()) {
		printf("Could not initialize headers.\n");
		fclose(target_fs);
		return EXIT_FAILURE;
	}

	// ------------------------------------------------------------------------
	// Initialize the mountpoints.
	if (!init_mount_points(argc, argv)) {
		printf("Could not initialize mount points.\n");
		fclose(target_fs);
		return EXIT_FAILURE;
	}

	// ------------------------------------------------------------------------
	// Create file headers.
	header_offset = sizeof(struct initrd_file_t) * MAX_FILES + sizeof(int);
	printf("Creating headers...\n");
	for (uint32_t i = 1; i < argc; ++i) {
		if (is_option_source(argv[i]) && ((i + 1) < argc)) {
			if (!create_file_headers(argv[i + 1], "")) {
				printf("Could not create file headers.\n");
				fclose(target_fs);
				return EXIT_FAILURE;
			}
		}
	}
	printf("[" GREEN "DONE" RESET "]\n\n");

	// ------------------------------------------------------------------------
	// Copying information about headers on filesystem.
	printf("%-64s", "Copying information about headers to filesystem...");
	fwrite(&header_idx, sizeof(int), 1, target_fs);
	fwrite(headers, sizeof(struct initrd_file_t), 32, target_fs);
	printf("[" GREEN "DONE" RESET "]\n\n");

	// ------------------------------------------------------------------------
	// Write headers on filesystem.
	for (uint32_t i = 1; i < argc; ++i) {
		if (is_option_source(argv[i]) && ((i + 1) < argc)) {
			if (!write_file_system(argv[i + 1])) {
				printf("Could not write on filesystem.\n");
				fclose(target_fs);
				return EXIT_FAILURE;
			}
		}
	}
	fclose(target_fs);
	return 0;
}
