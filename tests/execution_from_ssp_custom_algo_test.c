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

    int exitCode = 0;
    cosim_execution* execution = NULL;
    cosim_observer* observer = NULL;

    const char* dataDir = getenv("TEST_DATA_DIR");
    if (!dataDir) {
        fprintf(stderr, "Environment variable TEST_DATA_DIR not set\n");
        goto Lfailure;
    }

    char sspDir[1024];
    int rc = snprintf(sspDir, sizeof sspDir, "%s/ssp/demo/no_algorithm_element", dataDir);
    if (rc < 0) {
        perror(NULL);
        goto Lfailure;
    }

    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    execution = cosim_ssp_fixed_step_execution_create(sspDir, true, 0, nanoStepSize); // override ssp startTime
    if (!execution) { goto Lerror; }

    cosim_execution_status status;
    cosim_execution_get_status(execution, &status);

    if (status.current_time != 0.0) {
        fprintf(stderr, "Expected value 0.0, got %f\n", (double)(status.current_time / 1.0e9));
        goto Lfailure;
    }

    observer = cosim_last_value_observer_create();
    if (!observer) { goto Lerror; }
    cosim_execution_add_observer(execution, observer);

    rc = cosim_execution_step(execution, 3);
    if (rc < 0) { goto Lerror; }

    size_t numSlaves = cosim_execution_get_num_slaves(execution);

    cosim_slave_info infos[2];
    rc = cosim_execution_get_slave_infos(execution, &infos[0], numSlaves);
    if (rc < 0) { goto Lerror; }

    char name[SLAVE_NAME_MAX_SIZE];
    strcpy(name, "KnuckleBoomCrane");
    for (size_t i = 0; i < numSlaves; i++) {
        if (0 == strncmp(infos[i].name, name, 15)) {
            double value = -1;
            cosim_slave_index slaveIndex = infos[i].index;
            cosim_value_reference varIndex = 2;
            rc = cosim_observer_slave_get_real(observer, slaveIndex, &varIndex, 1, &value);
            if (rc < 0) {
                goto Lerror;
            }
            if (value != 0.05) {
                fprintf(stderr, "Expected value 0.05, got %f\n", value);
                goto Lfailure;
            }
        }
    }

    cosim_execution_start(execution);
    Sleep(100);
    cosim_execution_stop(execution);

    goto Lcleanup;

Lerror:
    print_last_error();

Lfailure:
    exitCode = 1;

Lcleanup:
    cosim_observer_destroy(observer);
    cosim_execution_destroy(execution);
    return exitCode;
}
