///                MentOS, The Mentoring Operating system project
/// @file cmd_logo.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "video.h"
#include "stdio.h"
#include "version.h"

#define LOGO_TAB "                  "
void cmd_logo(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	video_set_color(BRIGHT_GREEN);
	printf(LOGO_TAB " __  __                  _      ___    ____  \n");
	printf(LOGO_TAB "|  \\/  |   ___   _ __   | |_   / _ \\  / ___| \n");
	printf(LOGO_TAB "| |\\/| |  / _ \\ | '_ \\  | __| | | | | \\___ \\ \n");
	printf(LOGO_TAB "| |  | | |  __/ | | | | | |_  | |_| |  ___) |\n");
	printf(LOGO_TAB "|_|  |_|  \\___| |_| |_|  \\__|  \\___/  |____/ \n");
	video_set_color(BRIGHT_BLUE);
	printf("\n");
	printf(LOGO_TAB "              Welcome to ");
	video_set_color(WHITE);
	printf(OS_NAME "\n");
	video_set_color(BRIGHT_BLUE);
	printf(LOGO_TAB "  The ");
	video_set_color(WHITE);
	printf("Mentoring Operating System");
	video_set_color(BRIGHT_BLUE);
	printf(" ver. ");
	video_set_color(WHITE);
	printf(OS_VERSION "\n");
	printf("\n");
	video_set_color(WHITE);
}
