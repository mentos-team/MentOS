///                MentOS, The Mentoring Operating system project
/// @file shell_login.c
/// @brief Functions used to manage login.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "shell_login.h"
#include "vfs.h"
#include "stdio.h"
#include "fcntl.h"
#include "debug.h"
#include "video.h"
#include "string.h"
#include "keyboard.h"

/// @brief Contains the credentials retrieved from the file.
typedef struct credentials_t {
	/// The username.
	char username[CREDENTIALS_LENGTH];
	/// The password.
	char password[CREDENTIALS_LENGTH];
} credentials_t;

/// @brief Initialize the given credentials.
void init_credentials(credentials_t *credentials)
{
	if (credentials != NULL) {
		memset(credentials->username, '\0', CREDENTIALS_LENGTH);
		memset(credentials->password, '\0', CREDENTIALS_LENGTH);
	}
}

/// @brief
/// @param fd           The FD of the file which contains the credentials.
/// @param credentials  The structure which has to be filled with the
///                     credentials.
/// @return If the credentials has been retrieved.
bool_t user_get(int fd, struct credentials_t *credentials)
{
	// Create a support array of char.
	static char support[CREDENTIALS_LENGTH];
	// Cariable which will contain the number of bytes actually transferred.
	ssize_t bytes_read = 0;

	// Get the USERNAME.
	memset(support, '\0', CREDENTIALS_LENGTH);
	for (int it = 0; it < CREDENTIALS_LENGTH; ++it) {
		bytes_read = read(fd, &support[it], 1);
		if (support[it] == ':') {
			support[it] = '\0';

			break;
		}
	}
	replace_char(support, '\r', 0);
	if ((bytes_read == 0) || (strlen(support) == 0)) {
		return false;
	}
	strcpy(credentials->username, support);

	// Get the PASSWORD.
	memset(support, '\0', CREDENTIALS_LENGTH);
	for (int it = 0; it < CREDENTIALS_LENGTH; ++it) {
		bytes_read = read(fd, &support[it], 1);
		if ((support[it] == '\n') || (support[it] == EOF)) {
			support[it] = '\0';

			break;
		}
	}

	replace_char(support, '\r', 0);
	if ((bytes_read == 0) || (strlen(support) == 0)) {
		return false;
	}
	strcpy(credentials->password, support);

	return true;
}

/// @brief Checks if the given credentials are correct.
bool_t check_credentials(credentials_t *credentials)
{
	// Initialize a variable for the return value.
	bool_t status = false;

	/* Initialize the structure which will contain the username and the
     * password.
     */
	credentials_t existing;
	init_credentials(&existing);

	// Open the file which contains the credentials.
	// TODO: BUG: The first time the open is called, it fails.
	int fd = open("/passwd", O_RDONLY, 0);

	// Check the file descriptor.
	if (fd >= 0) {
		// Get the next row inside the file containing the credentials.
		while (user_get(fd, &existing) == true) {
			if (!strcmp(credentials->username, existing.username) &&
				!strcmp(credentials->password, existing.password)) {
				status = true;

				break;
			}
		}
		// Close the file descriptor.
		close(fd);
	} else {
		dbg_print("Can't open passwd file\n");
	}
	return status;
}

void shell_login()
{
	do {
		// Initialize the credentials.
		credentials_t credentials;
		init_credentials(&credentials);

		// Ask the username.
		printf("Username :");
		// Update the lower-bounds for the video.
		lower_bound_x = video_get_column();
		lower_bound_y = video_get_line();
		// Get the username.
		scanf("%50s", credentials.username);

		// Ask the password.
		printf("Password :");
		// Update the lower-bounds for the video.
		lower_bound_x = video_get_column();
		lower_bound_y = video_get_line();
		// Set the shadow option.
		keyboard_set_shadow(true);
		keyboard_set_shadow_character('*');
		// Get the password.
		scanf("%50s", credentials.password);
		// Disable the shadow option.
		keyboard_set_shadow(false);

		// Check if the data are correct.
		if (check_credentials(&credentials)) {
			strcpy(current_user.username, credentials.username);

			break;
		}
		printf("Sorry, try again.\n");
	} while (true);
}
