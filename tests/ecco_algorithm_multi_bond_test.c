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

#define MAX_NUMBER_OF_SLAVES 10

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
    char chassisFmuPath[1024];
    char wheelFmuPath[1024];
    char logConfigPath[1024];
    int64_t toTimePoint = (int64_t)(4 * 1e9);
    snprintf(chassisFmuPath, sizeof chassisFmuPath, "%s/fmi2/quarter_truck/Chassis.fmu", dataDir);
    snprintf(wheelFmuPath, sizeof wheelFmuPath, "%s/fmi2/quarter_truck/Wheel.fmu", dataDir);
    snprintf(logConfigPath, sizeof logConfigPath, "%s/fmi2/quarter_truck/LogConfig.xml", dataDir);

    cosim_execution* execution = NULL;
    cosim_slave* chassis = NULL;
    cosim_slave* wheel = NULL;
    cosim_observer* observer = NULL;
    double* cvo = NULL;
    double* wvi = NULL;
    double* cfi = NULL;
    double* wfo = NULL;
    cosim_time_point* times = NULL;
    cosim_step_number* steps = NULL;
    double* diffs = NULL;

    cosim_algorithm* ecco_algorithm = cosim_ecco_algorithm_create(
        0.8,
        1e-4,
        1e-4,
        0.01,
        0.2,
        1.5,
        1e-4,
        1e-4,
        0.2,
        0.15);

    execution = cosim_execution_create_v2(0, ecco_algorithm);
    if (!execution) {goto Lerror;}

    chassis = cosim_local_slave_create(chassisFmuPath, "chassis");
    if (!chassis) { goto Lerror; }

    wheel = cosim_local_slave_create(wheelFmuPath, "wheel");
    if (!wheel) { goto Lerror; }

    // observer = cosim_time_series_observer_create();
    observer = cosim_buffered_time_series_observer_create(500000);
    if (!observer) { goto Lerror; }

    int rc = cosim_execution_add_observer(execution, observer);
    if (rc < 0) { goto Lerror; }

    cosim_slave_index chassisIndex = cosim_execution_add_slave(execution, chassis);
    if (chassisIndex < 0) { goto Lerror; }

    cosim_slave_index wheelIndex = cosim_execution_add_slave(execution, wheel);
    if (wheelIndex < 0) { goto Lerror; }

    // IO connections
    cosim_value_reference chassisVelOut = 23;
    cosim_value_reference chassisFIn = 4;
    cosim_value_reference wheelFOut = 15;
    cosim_value_reference wheelVelIn = 7;

    rc = cosim_execution_connect_real_variables(execution, chassisIndex, chassisVelOut, wheelIndex, wheelVelIn);
    if (rc < 0) { goto Lerror; }
    rc = cosim_execution_connect_real_variables(execution, wheelIndex, wheelFOut, chassisIndex, chassisFIn);
    if (rc < 0) { goto Lerror; }

    // Power bond connections
    cosim_ecco_add_power_bond(ecco_algorithm, chassisIndex, chassisVelOut, chassisFIn, wheelIndex, wheelFOut, wheelVelIn);

    // Initial values
    rc = cosim_execution_set_real_initial_value(execution, chassisIndex, 8, 400); // mass
    if (rc < 0) { goto Lerror; }
    rc = cosim_execution_set_string_initial_value(execution, chassisIndex, 1, "Euler"); // solverType
    if (rc < 0) { goto Lerror; }
    rc = cosim_execution_set_real_initial_value(execution, chassisIndex, 21, 1e-5); // timeStep
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_set_real_initial_value(execution, wheelIndex, 13, 40); // mass
    if (rc < 0) { goto Lerror; }
    rc = cosim_execution_set_string_initial_value(execution, wheelIndex, 1, "Euler"); // solverType
    if (rc < 0) { goto Lerror; }
    rc = cosim_execution_set_real_initial_value(execution, wheelIndex, 28, 1e-5); // timeStep
    if (rc < 0) { goto Lerror; }

    // Start observers
    rc = cosim_observer_start_observing(observer, wheelIndex, COSIM_VARIABLE_TYPE_REAL, wheelVelIn);
    if (rc < 0) { goto Lerror; }
    rc = cosim_observer_start_observing(observer, chassisIndex, COSIM_VARIABLE_TYPE_REAL, chassisVelOut);
    if (rc < 0) { goto Lerror; }
    rc = cosim_observer_start_observing(observer, wheelIndex, COSIM_VARIABLE_TYPE_REAL, wheelFOut);
    if (rc < 0) { goto Lerror; }
    rc = cosim_observer_start_observing(observer, chassisIndex, COSIM_VARIABLE_TYPE_REAL, chassisFIn);
    if (rc < 0) { goto Lerror; }

    rc = cosim_execution_simulate_until(execution, toTimePoint);
    if (rc < 0) { goto Lerror; }

    cosim_step_number fromStep = 1;
    cosim_step_number stepNumbers[3];

    rc = cosim_observer_get_step_numbers(observer, chassisIndex, 0, toTimePoint, stepNumbers);
    if (rc < 0) { goto Lerror; }

    const size_t nSamples = stepNumbers[1] - stepNumbers[0];
    steps = (cosim_step_number*)malloc(nSamples * sizeof(cosim_step_number));
    times = (cosim_time_point*)malloc(nSamples * sizeof(cosim_time_point));
    cvo = (double*)malloc(nSamples * sizeof(double));
    cfi = (double*)malloc(nSamples * sizeof(double));
    wvi = (double*)malloc(nSamples * sizeof(double));
    wfo = (double*)malloc(nSamples * sizeof(double));
    int64_t samplesRead;

    samplesRead = cosim_observer_slave_get_real_samples(observer, chassisIndex, chassisVelOut, fromStep, nSamples, cvo, steps, times);
    if (samplesRead < 0) { goto Lerror; }
    samplesRead = cosim_observer_slave_get_real_samples(observer, chassisIndex, chassisFIn, fromStep, nSamples, cfi, steps, times);
    if (samplesRead < 0) { goto Lerror; }
    samplesRead = cosim_observer_slave_get_real_samples(observer, wheelIndex, wheelVelIn, fromStep, nSamples, wvi, steps, times);
    if (samplesRead < 0) { goto Lerror; }
    samplesRead = cosim_observer_slave_get_real_samples(observer, wheelIndex, wheelFOut, fromStep, nSamples, wfo, steps, times);
    if (samplesRead < 0) { goto Lerror; }

    const float threshold = 1e-2f;
    diffs = (double*)malloc(nSamples * sizeof(double));
    size_t ptr = 0;
    for (size_t i = 1; i < nSamples; ++i) {
        diffs[ptr++] = fabs((cvo[i] * cfi[i]) - (wvi[i] * wfo[i]));;
    }

    const size_t offset = 100;
    for (size_t i = (nSamples > offset ? nSamples - offset : 0); i < nSamples; i++) {
        if (diffs[i] > threshold) {
            fprintf(stderr, "Power bond mismatch at sample %zu: %f\n", i, diffs[i]);
            goto Lfailure;
        }
    }

    goto Lcleanup;

    Lerror:
    print_last_error();

    Lfailure:
    exitCode = 1;

    Lcleanup:
    cosim_observer_destroy(observer);
    cosim_local_slave_destroy(chassis);
    cosim_local_slave_destroy(wheel);
    cosim_execution_destroy(execution);
    free(cvo);
    free(wvi);
    free(cfi);
    free(wfo);
    free(times);
    free(diffs);

    return exitCode;
}
