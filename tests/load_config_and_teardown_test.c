#include <cosim.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WINDOWS
#    include <windows.h>
#else
#    include <unistd.h>
#    define Sleep(x) usleep((x)*1000)
#endif

void print_last_error()
{
    fprintf(
        stderr,
        "Error code %d: %s\n",
        cosim_last_error_code(), cosim_last_error_message());
}

int main()
{
    cosim_log_setup_simple_console_logging();
    cosim_log_set_output_level(COSIM_LOG_SEVERITY_INFO);

    const char* dataDir = getenv("TEST_DATA_DIR");
    if (!dataDir) {
        fprintf(stderr, "Environment variable TEST_DATA_DIR not set\n");
        return 1;
    }

    char sspDir[1024];
    int rc = snprintf(sspDir, sizeof sspDir, "%s/ssp/demo", dataDir);
    if (rc < 0) {
        perror(NULL);
        return 1;
    }

    cosim_execution* execution = cosim_ssp_execution_create(sspDir, false, 0);
    if (!execution) {
        print_last_error();
        return 1;
    }

    cosim_execution_destroy(execution);

    return 0;
}
