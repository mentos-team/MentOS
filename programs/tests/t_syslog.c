#include <syslog.h>
#include <unistd.h>

int main(void)
{
    // Open syslog connection with identifier "syslog_test"
    openlog("syslog_test", LOG_CONS | LOG_PID, LOG_USER);

    // Set log mask to allow only messages of priority LOG_WARNING and above
    setlogmask(LOG_UPTO(LOG_WARNING));

    // Log messages at different levels to test filtering
    syslog(LOG_DEBUG, "This is a debug message and should not appear.");
    syslog(LOG_INFO, "This is an info message and should not appear.");
    syslog(LOG_NOTICE, "This is a notice message and should not appear.");
    syslog(LOG_WARNING, "This is a warning message and should appear.");
    syslog(LOG_ERR, "This is an error message and should appear.");
    syslog(LOG_CRIT, "This is a critical message and should appear.");
    syslog(LOG_ALERT, "This is an alert message and should appear.");
    syslog(LOG_EMERG, "This is an emergency message and should appear.");

    // Close the syslog connection
    closelog();

    return 0;
}
