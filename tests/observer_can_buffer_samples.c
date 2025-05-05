#include <cosim.h>

#include <inttypes.h>
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

    int64_t nanoStepSize = (int64_t)(0.1 * 1.0e9);
    execution = cosim_execution_create(0, nanoStepSize);
    if (!execution) { goto Lerror; }

    slave = cosim_local_slave_create(fmuPath, "slave");
    if (!slave) { goto Lerror; }

    observer = cosim_time_series_observer_create();
    if (!observer) { goto Lerror; }

    cosim_slave_index slaveIndex = cosim_execution_add_slave(execution, slave);
    if (slaveIndex < 0) { goto Lerror; }

    rc = cosim_execution_add_observer(execution, observer);
    if (rc < 0) { goto Lerror; }

    manipulator = cosim_override_manipulator_create();
    if (!manipulator) { goto Lerror; }

    rc = cosim_execution_add_manipulator(execution, manipulator);
    if (rc < 0) { goto Lerror; }

    double inputRealSamples[10] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    int inputIntSamples[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    cosim_value_reference reference = 0;

    rc = cosim_observer_start_observing(observer, slaveIndex, COSIM_VARIABLE_TYPE_REAL, reference);
    if (rc < 0) { goto Lerror; }
    rc = cosim_observer_start_observing(observer, slaveIndex, COSIM_VARIABLE_TYPE_INTEGER, reference);
    if (rc < 0) { goto Lerror; }

    for (int i = 0; i < 10; i++) {
        rc = cosim_manipulator_slave_set_real(manipulator, 0, &reference, 1, &inputRealSamples[i]);
        if (rc < 0) { goto Lerror; }
        rc = cosim_manipulator_slave_set_integer(manipulator, 0, &reference, 1, &inputIntSamples[i]);
        if (rc < 0) { goto Lerror; }
        rc = cosim_execution_step(execution, 1);
        if (rc < 0) { goto Lerror; }
    }

    cosim_step_number fromStep = 1;
    const size_t nSamples = 10;
    double realSamples[10];
    int intSamples[10];
    cosim_time_point times[10];
    cosim_step_number steps[10];

    int64_t readRealSamples = cosim_observer_slave_get_real_samples(observer, slaveIndex, reference, fromStep, nSamples, realSamples, steps, times);
    if (readRealSamples != (int64_t)nSamples) {
        print_last_error();
        fprintf(stderr, "Expected to read 10 real samples, got %" PRId64 "\n", readRealSamples);
        goto Lfailure;
    }

    int64_t readIntSamples = cosim_observer_slave_get_integer_samples(observer, slaveIndex, reference, fromStep, nSamples, intSamples, steps, times);
    if (readIntSamples != (int64_t)nSamples) {
        print_last_error();
        fprintf(stderr, "Expected to read 10 int samples, got %" PRId64 "\n", readIntSamples);
        goto Lfailure;
    }

    long expectedSteps[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    double expectedRealSamples[10] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    int expectedIntSamples[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    double t[10] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    cosim_time_point expectedTimeSamples[10];
    for (int j = 0; j < 10; ++j) {
        expectedTimeSamples[j] = (cosim_time_point)(1.0e9 * t[j]);
    }

    for (int i = 0; i < 10; i++) {
        if (fabs(expectedRealSamples[i] - realSamples[i]) > 0.000001) {
            fprintf(stderr, "Sample nr %d expected real sample %lf, got %lf\n", i, expectedRealSamples[i], realSamples[i]);
            goto Lfailure;
        }
        if (expectedIntSamples[i] != intSamples[i]) {
            fprintf(stderr, "Sample nr %d expected int sample %d, got %d\n", i, expectedIntSamples[i], intSamples[i]);
            goto Lfailure;
        }
        if (expectedSteps[i] != steps[i]) {
            fprintf(stderr, "Sample nr %d expected step %li, got %" PRId64 "\n", i, expectedSteps[i], steps[i]);
            goto Lfailure;
        }
        if (expectedTimeSamples[i] != times[i]) {
            fprintf(stderr, "Sample nr %d expected time sample %" PRId64 ", got %" PRId64 "\n", i, expectedTimeSamples[i], times[i]);
            goto Lfailure;
        }
    }

    cosim_step_number nums[2];
    cosim_duration dur = (cosim_time_point)(0.5 * 1.0e9);
    rc = cosim_observer_get_step_numbers_for_duration(observer, 0, dur, nums);
    if (rc < 0) { goto Lerror; }
    if (nums[0] != 5) {
        fprintf(stderr, "Expected step number %i, got %" PRId64 "\n", 5, nums[0]);
        goto Lfailure;
    }
    if (nums[1] != 10) {
        fprintf(stderr, "Expected step number %i, got %" PRId64 "\n", 10, nums[1]);
        goto Lfailure;
    }

    cosim_time_point t1 = (cosim_time_point)(0.3 * 1e9);
    cosim_time_point t2 = (cosim_time_point)(0.6 * 1e9);
    rc = cosim_observer_get_step_numbers(observer, 0, t1, t2, nums);
    if (rc < 0) { goto Lerror; }
    if (nums[0] != 3) {
        fprintf(stderr, "Expected step number %i, got %" PRId64 "\n", 3, nums[0]);
        goto Lfailure;
    }
    if (nums[1] != 6) {
        fprintf(stderr, "Expected step number %i, got %" PRId64 "\n", 6, nums[1]);
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
