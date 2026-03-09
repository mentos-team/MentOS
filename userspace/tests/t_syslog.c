/// @file t_syslog.c
/// @brief Test program for syslog functionality.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <syslog.h>
#include <unistd.h>

int main(void)
{
    // Open syslog connection with identifier "syslog_test"
    openlog("syslog_test", LOG_CONS | LOG_PID, LOG_USER);

    // Set log mask to allow only messages of priority LOG_WARNING and above
    setlogmask(LOG_UPTO(LOG_WARNING));

    // Log messages at different levels to test filtering
    syslog(LOG_DEBUG, "[t_syslog] This is a debug message and should not appear.\n");
    syslog(LOG_INFO, "[t_syslog] This is an info message and should not appear.\n");
    syslog(LOG_NOTICE, "[t_syslog] This is a notice message and should not appear.\n");
    syslog(LOG_WARNING, "[t_syslog] This is a warning message and should appear.\n");
    syslog(LOG_ERR, "[t_syslog] This is an error message and should appear.\n");
    syslog(LOG_CRIT, "[t_syslog] This is a critical message and should appear.\n");
    syslog(LOG_ALERT, "[t_syslog] This is an alert message and should appear.\n");
    syslog(LOG_EMERG, "[t_syslog] This is an emergency message and should appear.\n");

    // Close the syslog connection
    closelog();

    return 0;
}
