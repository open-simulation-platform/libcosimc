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
    const char* dataDir = getenv("TEST_DATA_DIR");
    char fmuPath[1024];
    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    snprintf(fmuPath, sizeof fmuPath, "%s/fmi1/identity.fmu", dataDir);

    cosim_execution* execution = NULL;
    cosim_slave* slave1 = NULL;
    cosim_slave* slave2 = NULL;
    cosim_observer* observer = NULL;
    cosim_manipulator* manipulator = NULL;

    execution = cosim_execution_create(0, nanoStepSize);
    if (!execution) { goto Lerror; }

    slave1 = cosim_local_slave_create(fmuPath, "slave1");
    if (!slave1) { goto Lerror; }

    slave2 = cosim_local_slave_create(fmuPath, "slave2");
    if (!slave2) { goto Lerror; }

    observer = cosim_last_value_observer_create();
    if (!observer) { goto Lerror; }

    cosim_slave_index slaveIndex1 = cosim_execution_add_slave(execution, slave1);
    if (slaveIndex1 < 0) { goto Lerror; }

    cosim_slave_index slaveIndex2 = cosim_execution_add_slave(execution, slave2);
    if (slaveIndex2 < 0) { goto Lerror; }

    int rc = cosim_execution_add_observer(execution, observer);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_connect_real_variables(execution, slaveIndex1, 0, slaveIndex2, 0);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_connect_integer_variables(execution, slaveIndex1, 0, slaveIndex2, 0);
    if (rc < 0) { goto Lerror; }

    // Test that this fails
    rc = cosim_execution_connect_integer_variables(execution, slaveIndex1, 1, slaveIndex2, 1);
    if (rc != -1) {
        fprintf(stderr, "Expected to fail when connecting nonexistent variables");
        goto Lfailure;
    }

    manipulator = cosim_override_manipulator_create();
    if (!manipulator) { goto Lerror; }

    rc = cosim_execution_add_manipulator(execution, manipulator);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference realInVar = 0;
    const double realInVal = 5.0;
    rc = cosim_manipulator_slave_set_real(manipulator, slaveIndex1, &realInVar, 1, &realInVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference intInVar = 0;
    const int intInVal = 42;
    rc = cosim_manipulator_slave_set_integer(manipulator, slaveIndex1, &intInVar, 1, &intInVal);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_step(execution, 10);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference realOutVar = 0;
    double realOutVal = -1.0;
    rc = cosim_observer_slave_get_real(observer, slaveIndex2, &realOutVar, 1, &realOutVal);
    if (rc < 0) { goto Lerror; }

    cosim_value_reference intOutVar = 0;
    int intOutVal = -1;
    rc = cosim_observer_slave_get_integer(observer, slaveIndex2, &intOutVar, 1, &intOutVal);
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
    cosim_local_slave_destroy(slave2);
    cosim_local_slave_destroy(slave1);
    cosim_execution_destroy(execution);

    return exitCode;
}
