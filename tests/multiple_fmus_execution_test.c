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
    cosim_slave* slave1 = NULL;
    cosim_slave* slave2 = NULL;
    cosim_observer* observer1 = NULL;
    cosim_observer* observer2 = NULL;
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

    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    execution = cosim_execution_create(0, nanoStepSize);
    if (!execution) { goto Lerror; }

    slave1 = cosim_local_slave_create(fmuPath, "slave1");
    if (!slave1) { goto Lerror; }

    slave2 = cosim_local_slave_create(fmuPath, "slave2");
    if (!slave2) { goto Lerror; }

    observer1 = cosim_last_value_observer_create();
    if (!observer1) { goto Lerror; }

    observer2 = cosim_last_value_observer_create();
    if (!observer2) { goto Lerror; }

    manipulator = cosim_override_manipulator_create();
    if (!manipulator) { goto Lerror; }

    rc = cosim_execution_add_manipulator(execution, manipulator);
    if (rc < 0) { goto Lerror; }

    cosim_slave_index slave_index1 = cosim_execution_add_slave(execution, slave1);
    if (slave_index1 < 0) { goto Lerror; }
    cosim_slave_index slave_index2 = cosim_execution_add_slave(execution, slave2);
    if (slave_index2 < 0) { goto Lerror; }

    rc = cosim_execution_add_observer(execution, observer1);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_add_observer(execution, observer2);
    if (rc < 0) { goto Lerror; }


    cosim_value_reference realInVar = 0;
    const double realInVal = 5.0;
    rc = cosim_manipulator_slave_set_real(manipulator, slave_index1, &realInVar, 1, &realInVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference intInVar = 0;
    const int intInVal = 42;
    rc = cosim_manipulator_slave_set_integer(manipulator, slave_index1, &intInVar, 1, &intInVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference boolInVar = 0;
    const bool boolInVal = true;
    rc = cosim_manipulator_slave_set_boolean(manipulator, slave_index1, &boolInVar, 1, &boolInVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference strInVar = 0;
    const char* strInVal = "foo";
    rc = cosim_manipulator_slave_set_string(manipulator, slave_index1, &strInVar, 1, &strInVal);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_step(execution, 10);
    if (rc < 0) { goto Lerror; }

    cosim_execution_status executionStatus;
    rc = cosim_execution_get_status(execution, &executionStatus);
    if (rc < 0) { goto Lerror; }

    double precision = 1e-9;
    double simTime = executionStatus.current_time * 1e-9;
    if (fabs(simTime - 1.0) > precision) {
        fprintf(stderr, "Expected current time == 1.0 s, got %f\n", simTime);
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

    cosim_value_reference realOutVar = 0;
    double realOutVal = -1.0;
    rc = cosim_observer_slave_get_real(observer1, slave_index1, &realOutVar, 1, &realOutVal);
    if (rc < 0) { goto Lerror; }

    if (realOutVal != 5.0) {
        fprintf(stderr, "Expected value 5.0, got %f\n", realOutVal);
        goto Lfailure;
    }

    cosim_value_reference intOutVar = 0;
    int intOutVal = 10;
    rc = cosim_observer_slave_get_integer(observer1, slave_index1, &intOutVar, 1, &intOutVal);
    if (rc < 0) { goto Lerror; }

    if (intOutVal != 42) {
        fprintf(stderr, "Expected value 42, got %i\n", intOutVal);
        goto Lfailure;
    }

    cosim_value_reference boolOutVar = 0;
    bool boolOutVal = false;
    rc = cosim_observer_slave_get_boolean(observer1, slave_index1, &boolOutVar, 1, &boolOutVal);
    if (rc < 0) { goto Lerror; }

    if (boolOutVal != true) {
        fprintf(stderr, "Expected value true, got %s\n", boolOutVal > 0 ? "true" : "false");
        goto Lfailure;
    }

    cosim_value_reference strOutVar = 0;
    const char* strOutVal;
    rc = cosim_observer_slave_get_string(observer1, slave_index1, &strOutVar, 1, &strOutVal);
    if (rc < 0) { goto Lerror; }

    if (0 != strncmp(strOutVal, "foo", SLAVE_NAME_MAX_SIZE)) {
        fprintf(stderr, "Expected value foo, got %s\n", strOutVal);
        goto Lfailure;
    }

    rc = cosim_observer_slave_get_real(observer2, slave_index2, &realOutVar, 1, &realOutVal);
    if (rc < 0) { goto Lerror; }

    rc = cosim_observer_slave_get_integer(observer2, slave_index2, &intOutVar, 1, &intOutVal);
    if (rc < 0) { goto Lerror; }

    if (realOutVal != 0.0) {
        fprintf(stderr, "Expected value 0.0, got %f\n", realOutVal);
        goto Lfailure;
    }
    if (intOutVal != 0) {
        fprintf(stderr, "Expected value 0, got %i\n", intOutVal);
        goto Lfailure;
    }

    goto Lcleanup;

Lerror:
    print_last_error();

Lfailure:
    exitCode = 1;

Lcleanup:
    cosim_manipulator_destroy(manipulator);
    cosim_observer_destroy(observer1);
    cosim_observer_destroy(observer2);
    cosim_local_slave_destroy(slave2);
    cosim_local_slave_destroy(slave1);
    cosim_execution_destroy(execution);

    return exitCode;
}
