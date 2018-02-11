#define LOG_PID 0
#define LOG_CONS 0
#define LOG_USER 0
#define LOG_ERR 0
#define LOG_WARNING 0
#define LOG_INFO 0
#define LOG_DEBUG 0

void openlog(const char* appname, int flags, int flags2);
void syslog(int level, const char* format, const char* message);
void closelog();