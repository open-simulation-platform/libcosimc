#include <cosim.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
    cosim_slave* slave = NULL;
    cosim_observer* observer = NULL;
    cosim_manipulator* manipulator = NULL;

    const char* dataDir = getenv("TEST_DATA_DIR");
    if (!dataDir) {
        fprintf(stderr, "Environment variable TEST_DATA_DIR not set\n");
        goto Lfailure;
    }

    char fmuPath[1024];
    int rc = snprintf(fmuPath, sizeof fmuPath, "%s/fmi1/identity.fmu", dataDir);
    if (rc < 0) {
        perror(NULL);
        goto Lfailure;
    }

    // ===== Can step n times and get status
    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    execution = cosim_execution_create(0, nanoStepSize);
    if (!execution) { goto Lerror; }

    slave = cosim_local_slave_create(fmuPath, "slave");
    if (!slave) { goto Lerror; }

    observer = cosim_last_value_observer_create();
    if (!observer) { goto Lerror; }

    cosim_slave_index slave_index = cosim_execution_add_slave(execution, slave);
    if (slave_index < 0) { goto Lerror; }

    rc = cosim_execution_add_observer(execution, observer);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_step(execution, 10);
    if (rc < 0) { goto Lerror; }

    cosim_execution_status executionStatus;
    rc = cosim_execution_get_status(execution, &executionStatus);
    if (rc < 0) { goto Lerror; }

    double precision = 1e-9;
    double simTime = executionStatus.current_time * 1e-9;
    if (fabs(simTime - 1.0) > precision) {
        fprintf(stderr, "Expected current time == 1.0, got %f\n", simTime);
        goto Lfailure;
    }

    if (executionStatus.state != COSIM_EXECUTION_STOPPED) {
        fprintf(stderr, "Expected state == %i, got %i\n", COSIM_EXECUTION_STOPPED, executionStatus.state);
        goto Lfailure;
    }

    if (executionStatus.error_code != COSIM_ERRC_SUCCESS) {
        fprintf(stderr, "Expected error code == %i, got %i\n", COSIM_ERRC_SUCCESS, executionStatus.error_code);
        goto Lfailure;
    }

    manipulator = cosim_override_manipulator_create();
    if (!manipulator) { goto Lerror; }

    rc = cosim_execution_add_manipulator(execution, manipulator);
    if (rc < 0) { goto Lerror; }

    // ===== Can start/stop execution and get status
    cosim_value_reference realInVar = 0;
    const double realInVal = 5.0;
    rc = cosim_manipulator_slave_set_real(manipulator, slave_index, &realInVar, 1, &realInVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference intInVar = 0;
    const int intInVal = 42;
    rc = cosim_manipulator_slave_set_integer(manipulator, slave_index, &intInVar, 1, &intInVal);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_start(execution);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_get_status(execution, &executionStatus);
    if (rc < 0) { goto Lerror; }

    if (executionStatus.state != COSIM_EXECUTION_RUNNING) {
        fprintf(stderr, "Expected state == %i, got %i\n", COSIM_EXECUTION_RUNNING, executionStatus.state);
        goto Lfailure;
    }

    if (executionStatus.error_code != COSIM_ERRC_SUCCESS) {
        fprintf(stderr, "Expected error code == %i, got %i\n", COSIM_ERRC_SUCCESS, executionStatus.error_code);
        goto Lfailure;
    }

    Sleep(100);

    rc = cosim_execution_stop(execution);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_get_status(execution, &executionStatus);
    if (rc < 0) { goto Lerror; }

    if (executionStatus.state != COSIM_EXECUTION_STOPPED) {
        fprintf(stderr, "Expected state == %i, got %i\n", COSIM_EXECUTION_STOPPED, executionStatus.state);
        goto Lfailure;
    }

    if (executionStatus.error_code != COSIM_ERRC_SUCCESS) {
        fprintf(stderr, "Expected error code == %i, got %i\n", COSIM_ERRC_SUCCESS, executionStatus.error_code);
        goto Lfailure;
    }

    cosim_value_reference realOutVar = 0;
    double realOutVal = -1.0;
    rc = cosim_observer_slave_get_real(observer, slave_index, &realOutVar, 1, &realOutVal);
    if (rc < 0) { goto Lerror; }


    cosim_value_reference intOutVar = 0;
    int intOutVal = 10;
    rc = cosim_observer_slave_get_integer(observer, slave_index, &intOutVar, 1, &intOutVal);
    if (rc < 0) { goto Lerror; }

    if (realOutVal != 5.0) {
        fprintf(stderr, "Expected value 5.0, got %f\n", realOutVal);
        goto Lfailure;
    }
    if (intOutVal != 42) {
        fprintf(stderr, "Expected value 42, got %i\n", intOutVal);
        goto Lfailure;
    }

    goto Lcleanup;

Lerror:
    print_last_error();

Lfailure:
    exitCode = 1;

Lcleanup:
    cosim_manipulator_destroy(manipulator);
    cosim_observer_destroy(observer);
    cosim_local_slave_destroy(slave);
    cosim_execution_destroy(execution);

    return exitCode;
}
