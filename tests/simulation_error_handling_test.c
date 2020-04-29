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
    cosim_manipulator* manipulator = NULL;
    cosim_slave* slave = NULL;

    const char* dataDir = getenv("TEST_DATA_DIR");
    if (!dataDir) {
        fprintf(stderr, "Environment variable TEST_DATA_DIR not set\n");
        goto Lfailure;
    }

    char fmuPath[1024];
    int rc = snprintf(fmuPath, sizeof fmuPath, "%s/fmi2/fail.fmu", dataDir);
    if (rc < 0) {
        perror(NULL);
        goto Lfailure;
    }

    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    execution = cosim_execution_create(0, nanoStepSize);
    if (!execution) { goto Lerror; }

    slave = cosim_local_slave_create(fmuPath, "slave");
    if (!slave) { goto Lerror; }

    cosim_slave_index slave_index = cosim_execution_add_slave(execution, slave);
    if (slave_index < 0) { goto Lerror; }

    manipulator = cosim_override_manipulator_create();
    if (!manipulator) { goto Lerror; }

    rc = cosim_execution_add_manipulator(execution, manipulator);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_step(execution, 1);
    if (rc < 0) { goto Lerror; }

    cosim_execution_status status;
    rc = cosim_execution_get_status(execution, &status);
    if (rc < 0) {
        fprintf(stderr, "Expected call to cosim_execution_get_status() 1 to return success.");
        goto Lfailure;
    }

    rc = cosim_execution_start(execution);
    if (rc < 0) { goto Lerror; }

    Sleep(100);

    rc = cosim_execution_get_status(execution, &status);
    if (rc < 0) {
        fprintf(stderr, "Expected call to cosim_execution_get_status() 2 to return success.");
        goto Lfailure;
    }

    cosim_value_reference ref = 0;
    const bool val = true;
    // Produces a model error in the subsequent step
    rc = cosim_manipulator_slave_set_boolean(manipulator, slave_index, &ref, 1, &val);
    if (rc < 0) { goto Lerror; }

    // Need to wait a bit due to stepping (and failure) happening in another thread.
    Sleep(400);

    rc = cosim_execution_get_status(execution, &status);
    if (rc >= 0) {
        fprintf(stderr, "Expected call to cosim_execution_get_status() 3 to return failure.");
        goto Lfailure;
    }

    rc = cosim_execution_get_status(execution, &status);
    if (rc >= 0) {
        fprintf(stderr, "Expected call to cosim_execution_get_status() 4 to return failure.");
        goto Lfailure;
    }

    if (status.state != COSIM_EXECUTION_ERROR) {
        fprintf(stderr, "Expected state == %i, got %i\n", COSIM_EXECUTION_ERROR, status.state);
        goto Lfailure;
    }

    print_last_error();
    const char* lastErrorMessage = cosim_last_error_message();
    if (0 == strncmp(lastErrorMessage, "", 1)) {
        fprintf(stdout, "Expected to find an error message, but last error was: %s\n", lastErrorMessage);
        goto Lfailure;
    }
    int lastErrorCode = cosim_last_error_code();
    if (lastErrorCode != COSIM_ERRC_SIMULATION_ERROR) {
        fprintf(stdout, "Expected to find error code %i, but got error code: %i\n", COSIM_ERRC_SIMULATION_ERROR, lastErrorCode);
        goto Lfailure;
    }

    // What do we expect should happen if calling further methods?
    Sleep(100);
    rc = cosim_execution_stop(execution);
    if (rc >= 0) { goto Lfailure; }

    goto Lcleanup;

Lerror:
    print_last_error();

Lfailure:
    exitCode = 1;

Lcleanup:
    cosim_manipulator_destroy(manipulator);
    cosim_local_slave_destroy(slave);
    cosim_execution_destroy(execution);
    return exitCode;
}
