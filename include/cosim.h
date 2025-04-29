/**
 *  \file
 *  Main header file
 *  \copyright
 *      This Source Code Form is subject to the terms of the Mozilla Public
 *      License, v. 2.0. If a copy of the MPL was not distributed with this
 *      file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef COSIM_H
#define COSIM_H

#ifndef __cplusplus
#    include <stdbool.h>
#endif
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/// The type used to specify (simulation) time points. The time unit is nanoseconds.
typedef int64_t cosim_time_point;

/// The type used to specify (simulation) time durations. The time unit is nanoseconds.
typedef int64_t cosim_duration;

/// value reference.
typedef uint32_t cosim_value_reference;

/// Slave index.
typedef int cosim_slave_index;

/// Step number
typedef int64_t cosim_step_number;

/// Error codes.
typedef enum
{
    COSIM_ERRC_SUCCESS = 0,

    // --- Codes unique to the C API ---

    /// Unspecified error (but message may contain details).
    COSIM_ERRC_UNSPECIFIED,

    /// Error reported by C/C++ runtime; check `errno` to get the right code.
    COSIM_ERRC_ERRNO,

    /// Invalid function argument.
    COSIM_ERRC_INVALID_ARGUMENT,

    /// Function may not be called while in this state.
    COSIM_ERRC_ILLEGAL_STATE,

    /// Index out of range.
    COSIM_ERRC_OUT_OF_RANGE,

    /**
     *  The time step failed, but can be retried with a shorter step length
     *  (if supported by all slaves).
     */
    COSIM_ERRC_STEP_TOO_LONG,

    // --- Codes that correspond to C++ API error conditions ---

    /// An input file is corrupted or invalid.
    COSIM_ERRC_BAD_FILE,

    /// The requested feature (e.g. an FMI feature) is unsupported.
    COSIM_ERRC_UNSUPPORTED_FEATURE,

    /// Error loading dynamic library (e.g. model code).
    COSIM_ERRC_DL_LOAD_ERROR,

    /// The model reported an error.
    COSIM_ERRC_MODEL_ERROR,

    /// An error occured during simulation.
    COSIM_ERRC_SIMULATION_ERROR,

    /// ZIP file error.
    COSIM_ERRC_ZIP_ERROR,
} cosim_errc;


/**
 *  Returns the error code associated with the last reported error.
 *
 *  Most functions in this library will indicate that an error occurred by
 *  returning -1 or `NULL`, after which this function can be called to
 *  obtain more detailed information about the problem.
 *
 *  This function must be called from the thread in which the error occurred,
 *  and before any new calls to functions in this library (with the exception
 *  of `cosim_last_error_message()`).
 *
 *  \returns
 *      The error code associated with the last reported error.
 */
cosim_errc cosim_last_error_code();


/**
 *  Returns a textual description of the last reported error.
 *
 *  Most functions in this library will indicate that an error occurred by
 *  returning -1 or `NULL`, after which this function can be called to
 *  obtain more detailed information about the problem.
 *
 *  This function must be called from the thread in which the error occurred,
 *  and before any new calls to functions in this library (with the exception
 *  of `cosim_last_error_code()`).
 *
 *  \returns
 *      A textual description of the last reported error.  The pointer is
 *      only guaranteed to remain valid until the next time a function in
 *      this library is called (with the exception of `cosim_last_error_code()`).
 */
const char* cosim_last_error_message();


struct cosim_execution_s;

/// An opaque object which contains the state for an execution.
typedef struct cosim_execution_s cosim_execution;


struct cosim_algorithm_s;

/// An opaque object which contains the configuration for a cosimulation algorithm.
typedef struct cosim_algorithm_s cosim_algorithm;

/**
 *  Creates a new execution.
 * 
 *  \param [in] startTime
 *      The (logical) time point at which the simulation should start.
 *  \param [in] stepSize
 *      The execution step size.
 *  \returns
 *      A pointer to an object which holds the execution state,
 *      or NULL on error.
 */
cosim_execution* cosim_execution_create(
    cosim_time_point startTime,
    cosim_duration stepSize);

/**
 * Creates an ecco algorithm with the specified parameters.
 * \param [in] safetyFactor Safety factor
 * \param [in] stepSize Initial step size
 * \param [in] minStepSize Minimum step size
 * \param [in] maxStepSize Maximum step size
 * \param [in] minChangeRate Minimum rate of change in step size
 * \param [in] maxChangeRate Maximum rate of change in step size
 * \param [in] absTolerance Absolute tolerance for deciding mismatch in the residual power
 * \param [in] relTolerance Relative tolerance for deciding mismatch in the residual power
 * \param [in] pGain Proportional value in the PI controller
 * \param [in] iGain Integral value in the PI controller
 * \returns A pointer to a new instance of cosim_algorithm, or NULL if an error occurred.
 */
cosim_algorithm* cosim_ecco_algorithm_create(
    double safetyFactor,
    double stepSize,
    double minStepSize,
    double maxStepSize,
    double minChangeRate,
    double maxChangeRate,
    double absTolerance,
    double relTolerance,
    double pGain,
    double iGain);

/**
 * Creates a power bond between two instances of models
 * \param [in] Ecco An algorithm instance
 * \param [in] index1 Slave index for the first model
 * \param [in] v1 The output of the first model
 * \param [in] u1 The input of the first model
 * \param [in] index2 Slave index Id for the second model
 * \param [in] v2 The output of the second model
 * \param [in] u2 The input of the second model
 * \returns
 *      0 on success and -1 on error.
 */
int cosim_ecco_add_power_bond(
    cosim_algorithm* algo,
    cosim_slave_index m1Index,
    cosim_value_reference v1,
    cosim_value_reference u1,
    cosim_slave_index m2Index,
    cosim_value_reference v2,
    cosim_value_reference u2);

/**
 * Creates a fixed step algorithm
 *  \param [in] stepSize
 *      The execution step size.
 * \returns A pointer to a new instance of cosim_algorithm, or NULL if an error occurred.
 */
cosim_algorithm* cosim_fixed_step_algorithm_create(cosim_duration stepSize);

/**
 *  Creates a new execution.
 *
 *  \param [in] startTime
 *      The (logical) time point at which the simulation should start.
 *  \param [in] algo*
 *      Co-simulation algorithm object
 *  \returns
 *      A pointer to an object which holds the execution state,
 *      or NULL on error.
 */
cosim_execution* cosim_execution_create_v2(cosim_time_point startTime, cosim_algorithm* algo);

/**
 *  Creates a new execution based on an OspSystemStructure.xml file.
 *
 *  \param [in] configPath
 *      Path to an OspSystemStructure.xml file, or a directory holding OspSystemStructure.xml
 *  \param [in] startTimeDefined
 *      Defines whether or not the following startTime variable should be ignored or not.
 *  \param [in] startTime
 *      The (logical) time point at which the simulation should start.
 *      If startTimeDefined=false, this variable will be ignored and a default value will be used.
 *  \returns
 *      A pointer to an object which holds the execution state,
 *      or NULL on error.
 */
cosim_execution* cosim_osp_config_execution_create(
    const char* configPath,
    bool startTimeDefined,
    cosim_time_point startTime);

/**
 *  Creates a new execution based on a SystemStructure.ssd file.
 *
 *  \param [in] sspDir
 *      Path to an .ssd file, or a directory holding SystemStructure.ssd
 *  \param [in] startTimeDefined
 *      Defines whether or not the following startTime variable should be ignored or not.
 *  \param [in] startTime
 *      The (logical) time point at which the simulation should start.
 *      If startTimeDefined=false, this variable will be ignored and a default value will be used.
 *  \returns
 *      A pointer to an object which holds the execution state,
 *      or NULL on error.
 */
cosim_execution* cosim_ssp_execution_create(
    const char* sspDir,
    bool startTimeDefined,
    cosim_time_point startTime);

/**
 *  Creates a new execution based on a SystemStructure.ssd file.
 *
 *  \param [in] sspDir
 *      Path to the directory holding SystemStructure.ssd
 *  \param [in] startTimeDefined
 *      Defines whether or not the following startTime variable should be ignored or not.
 *  \param [in] startTime
 *      The (logical) time point at which the simulation should start.
 *      If startTimeDefined=false, this variable will be ignored and a default value will be used.
 *  \param [in] stepSize
 *      The stepSize that will be used by the (fixed-step) co-simulation algorithm.
 *  \returns
 *      A pointer to an object which holds the execution state,
 *      or NULL on error.
 */
cosim_execution* cosim_ssp_fixed_step_execution_create(
    const char* sspDir,
    bool startTimeDefined,
    cosim_time_point startTime,
    cosim_duration stepSize);

/**
 *  Destroys an execution.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_destroy(cosim_execution* execution);

struct cosim_slave_s;

/// An opaque object which contains the state for a slave.
typedef struct cosim_slave_s cosim_slave;


/**
 *  Creates a new local slave.
 *
 *  \param [in] fmuPath
 *      Path to FMU.
 *  \param [in] instanceName
 *      Unique name of the instance.
 *
 *  \returns
 *      A pointer to an object which holds the local slave object,
 *      or NULL on error.
 */
cosim_slave* cosim_local_slave_create(const char* fmuPath, const char* instanceName);

/**
 *  Sets a real initial value for the given slave in the given execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param[in] slaveIndex
 *      The slave.
 *  \param vr
 *      The value_reference.
 *  \param value
 *      The initial value.
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_set_real_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, double value);

/**
 *  Sets a integer initial value for the given slave in the given execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param[in] slaveIndex
 *      The slave.
 *  \param vr
 *      The value_reference.
 *  \param value
 *      The initial value.
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_set_integer_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, int value);

/**
 *  Sets a boolean initial value for the given slave in the given execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param[in] slaveIndex
 *      The slave.
 *  \param vr
 *      The value_reference.
 *  \param value
 *      The initial value.
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_set_boolean_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, bool value);

/**
 *  Sets a string initial value for the given slave in the given execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param[in] slaveIndex
 *      The slave.
 *  \param vr
 *      The value_reference.
 *  \param value
 *      The initial value.
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_set_string_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, char* value);


/**
 *  Destroys a local slave.
 *
 *  \returns
 *       0 on success and -1 on error.
 */
int cosim_local_slave_destroy(cosim_slave* slave);


/**
 *  Loads a co-simulation FMU, instantiates a slave based on it, and adds it
 *  to an execution.
 *
 *  The slave is owned by the execution and is destroyed along with it.
 *
 *  \param [in] execution
 *      The execution to which the slave should be added.
 *  \param [in] slave
 *      A pointer to a slave, which may not be null. The slave may not previously have been added to any execution.
 *
 *  \returns
 *      The slave's unique index in the execution, or -1 on error.
 */
cosim_slave_index cosim_execution_add_slave(
    cosim_execution* execution,
    cosim_slave* slave);


/**
 *  Advances an execution a number of time steps.
 *
 *  \param [in] execution
 *      The execution to be stepped.
 *  \param [in] numSteps
 *      The number of steps to advance the simulation execution.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_step(cosim_execution* execution, size_t numSteps);

/**
 *  Advances an execution to a specific point in time (blocking).
 *
 *  \param [in] execution
 *      The execution to be stepped.
 *  \param [in] targetTime
 *      The point in time, which to advance the simulation execution.
 *
 *  \returns
 *      -1 on error, 0 if the simulation was stopped prior to reaching the specified targetTime
 *      and 1 if the simulation was successfully advanced to the specified targetTime.
 */
int cosim_execution_simulate_until(cosim_execution* execution, cosim_time_point targetTime);


/**
 *  Starts an execution (non blocking).
 *
 *  The execution will run until `cosim_execution_stop()` is called. The status of the
 *  simulation can be polled with `cosim_execution_get_status()`.
 *
 *  \param [in] execution
 *      The execution to be started.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_start(cosim_execution* execution);

/**
 *  Stops an execution.
 *
 *  \param [in] execution
 *      The execution to be stopped.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_stop(cosim_execution* execution);


/// Enables real time simulation for an execution.
int cosim_execution_enable_real_time_simulation(cosim_execution* execution);

/// Disables real time simulation for an execution.
int cosim_execution_disable_real_time_simulation(cosim_execution* execution);

/// Sets a custom real time factor.
int cosim_execution_set_real_time_factor_target(cosim_execution* execution, double realTimeFactor);

/// Sets the number of steps to monitor for rolling average real time factor measurement.
int cosim_execution_set_steps_to_monitor(cosim_execution* execution, int stepsToMonitor);


/// Execution states.
typedef enum
{
    COSIM_EXECUTION_STOPPED,
    COSIM_EXECUTION_RUNNING,
    COSIM_EXECUTION_ERROR
} cosim_execution_state;

/// A struct containing the execution status.
typedef struct
{
    /// Current simulation time.
    cosim_time_point current_time;
    /// Current execution state.
    cosim_execution_state state;
    /// Last recorded error code.
    int error_code;
    /// Total average real time factor.
    double total_average_real_time_factor;
    /// Rolling average real time factor.
    double rolling_average_real_time_factor;
    /// Current real time factor target.
    double real_time_factor_target;
    /// Executing towards real time target.
    int is_real_time_simulation;
    /// Number of steps used in rolling average real time factor measurement.
    int steps_to_monitor;
} cosim_execution_status;

/**
 * Returns execution status.
 *
 * This method will also poll the status of any asynchronous execution started
 * by calling `cosim_execution_start()`. Will return failure if a simulation
 * error occured during the execution, in which case the `status` parameter
 * will still be valid.
 *
 *  \param [in] execution
 *      The execution to get status from.
 *  \param [out] status
 *      A pointer to a cosim_execution_status that will be filled with actual
 *      execution status.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_get_status(
    cosim_execution* execution,
    cosim_execution_status* status);

/// Max number of characters used for slave name and source.
#define SLAVE_NAME_MAX_SIZE 1024

/// Variable types.
typedef enum
{
    COSIM_VARIABLE_TYPE_REAL,
    COSIM_VARIABLE_TYPE_INTEGER,
    COSIM_VARIABLE_TYPE_STRING,
    COSIM_VARIABLE_TYPE_BOOLEAN,
} cosim_variable_type;

/// Variable causalities.
typedef enum
{
    COSIM_VARIABLE_CAUSALITY_INPUT,
    COSIM_VARIABLE_CAUSALITY_PARAMETER,
    COSIM_VARIABLE_CAUSALITY_OUTPUT,
    COSIM_VARIABLE_CAUSALITY_CALCULATEDPARAMETER,
    COSIM_VARIABLE_CAUSALITY_LOCAL,
    COSIM_VARIABLE_CAUSALITY_INDEPENDENT
} cosim_variable_causality;

/// Variable variabilities.
typedef enum
{
    COSIM_VARIABLE_VARIABILITY_CONSTANT,
    COSIM_VARIABLE_VARIABILITY_FIXED,
    COSIM_VARIABLE_VARIABILITY_TUNABLE,
    COSIM_VARIABLE_VARIABILITY_DISCRETE,
    COSIM_VARIABLE_VARIABILITY_CONTINUOUS
} cosim_variable_variability;

/// A struct containing metadata for a variable.
typedef struct
{
    /// The name of the variable.
    char name[SLAVE_NAME_MAX_SIZE];
    /// The value reference.
    cosim_value_reference reference;
    /// The variable type.
    cosim_variable_type type;
    /// The variable causality.
    cosim_variable_causality causality;
    /// The variable variability.
    cosim_variable_variability variability;
} cosim_variable_description;

/// Returns the number of variables for a slave which has been added to an execution, or -1 on error.
int cosim_slave_get_num_variables(cosim_execution* execution, cosim_slave_index slave);

/**
 *  Returns variable metadata for a slave.
 *
 *  \param [in] execution
 *      The execution which the slave has been added to.
 *  \param [in] slave
 *      The index of the slave.
 *  \param [out] variables
 *      A pointer to an array of length `numVariables` which will be filled with actual `cosim_variable_description` values.
 *  \param [in] numVariables
 *      The length of the `variables` array.
 *
 *  \returns
 *      The number of variables written to `variables` array or -1 on error.
 */
int cosim_slave_get_variables(cosim_execution* execution, cosim_slave_index slave, cosim_variable_description variables[], size_t numVariables);

/// Returns the number of variables in the execution that currently has an active modifier (all slaves).
int cosim_get_num_modified_variables(cosim_execution* execution);

/// A struct containing information about a slave which has been added to an execution.
typedef struct
{
    /// The slave instance name.
    char name[SLAVE_NAME_MAX_SIZE];
    /// The slave's unique index in the exeuction.
    cosim_slave_index index;
} cosim_slave_info;

/// A struct containing variable information.
typedef struct
{
    /// The index of the slave containing the variable.
    cosim_slave_index slave_index;
    /// The type of the variable.
    cosim_variable_type type;
    /// The variable's value reference.
    cosim_value_reference value_reference;
} cosim_variable_id;


/// Returns the number of slaves which have been added to an execution.
size_t cosim_execution_get_num_slaves(cosim_execution* execution);

/**
 *  Returns slave infos.
 *
 *  \param [in] execution
 *      The execution to get slave infos from.
 *  \param [out] infos
 *      A pointer to an array of length `num_slaves` which will be filled with actual `slave_info` values.
 *  \param [in] numSlaves
 *      The length of the `infos` array.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_get_slave_infos(cosim_execution* execution, cosim_slave_info infos[], size_t numSlaves);


// Observer
struct cosim_observer_s;

/// An opaque object which contains the state for an observer.
typedef struct cosim_observer_s cosim_observer;

// Manipulator
struct cosim_manipulator_s;

/// An opaque object which contains the state for a manipulator.
typedef struct cosim_manipulator_s cosim_manipulator;


/**
 *  Sets the values of real variables for one slave.
 *
 *  \param [in] manipulator
 *      The manipulator.
 *  \param [in] slaveIndex
 *      The slave.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to set.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_manipulator_slave_set_real(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const double values[]);

/**
 *  Sets the values of integer variables for one slave.
 *
 *  \param [in] manipulator
 *      The manipulator.
 *  \param [in] slaveIndex
 *      The slave.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to set.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_manipulator_slave_set_integer(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const int values[]);

/**
 *  Sets the values of boolean variables for one slave.
 *
 *  \param [in] manipulator
 *      The manipulator.
 *  \param [in] slaveIndex
 *      The slave.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to set.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_manipulator_slave_set_boolean(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const bool values[]);

/**
 *  Sets the values of string variables for one slave.
 *
 *  \param [in] manipulator
 *      The manipulator.
 *  \param [in] slaveIndex
 *      The slave.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to set.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_manipulator_slave_set_string(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const char* values[]);

/**
 *  Resets any previously overridden variable values of a certain type for one slave.
 *
 *  \param [in] manipulator
 *      The manipulator.
 *  \param [in] slaveIndex
 *      The slave.
 *  \param [in] type
 *      The variable type.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to reset.
 *  \param [in] nv
 *      The length of the `variables` array.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_manipulator_slave_reset(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    cosim_variable_type type,
    const cosim_value_reference variables[],
    size_t nv);

/**
 *  Retrieves the values of real variables for one slave.
 *
 *  \param [in] observer
 *      The observer.
 *  \param [in] slave
 *      The slave.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to retrieve.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` which will be filled with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_observer_slave_get_real(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    double values[]);

/**
 *  Retrieves the values of integer variables for one slave.
 *
 *  \param [in] observer
 *      The observer.
 *  \param [in] slave
 *      The slave index.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to retrieve.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` which will be filled with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_observer_slave_get_integer(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    int values[]);

/**
 *  Retrieves the values of boolean variables for one slave.
 *
 *  \param [in] observer
 *      The observer.
 *  \param [in] slave
 *      The slave index.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to retrieve.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` which will be filled with the
 *      values of the variables specified in `variables`, in the same order.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_observer_slave_get_boolean(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    bool values[]);

/**
 *  Retrieves the values of string variables for one slave.
 *
 *  \param [in] observer
 *      The observer.
 *  \param [in] slave
 *      The slave index.
 *  \param [in] variables
 *      A pointer to an array of length `nv` that contains the (slave-specific)
 *      value references of variables to retrieve.
 *  \param [in] nv
 *      The length of the `variables` and `values` arrays.
 *  \param [out] values
 *      A pointer to an array of length `nv` which will be filled with pointers
 *      to the values of the variables specified in `variables`, in the same order.
 *      The pointers are valid until the next call to `cosim_observer_slave_get_string()`.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_observer_slave_get_string(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    const char* values[]);

/**
 * Retrieves a series of observed values, step numbers and times for a real variable.
 *
 * \param [in] observer the observer
 * \param [in] slave index of the slave
 * \param [in] valueReference the value reference
 * \param [in] fromStep the step number to start from
 * \param [in] nSamples the number of samples to read
 * \param [out] values the series of observed values
 * \param [out] steps the corresponding step numbers
 * \param [out] times the corresponding simulation times
 *
 * \returns
 *      The number of samples actually read, which may be smaller than `nSamples`.
 */
int64_t cosim_observer_slave_get_real_samples(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_value_reference valueReference,
    cosim_step_number fromStep,
    size_t nSamples,
    double values[],
    cosim_step_number steps[],
    cosim_time_point times[]);

/**
 * Retrieves a series of observed values, step numbers and times for an integer variable.
 *
 * \param [in] observer the observer
 * \param [in] slave index of the slave
 * \param [in] valueReference the value reference
 * \param [in] fromStep the step number to start from
 * \param [in] nSamples the number of samples to read
 * \param [out] values the series of observed values
 * \param [out] steps the corresponding step numbers
 * \param [out] times the corresponding simulation times
 *
 * \returns
 *      The number of samples actually read, which may be smaller than `nSamples`.
 */
int64_t cosim_observer_slave_get_integer_samples(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_value_reference valueReference,
    cosim_step_number fromStep,
    size_t nSamples,
    int values[],
    cosim_step_number steps[],
    cosim_time_point times[]);

/**
 * Retrieves two time-synchronized series of observed values for two real variables.
 *
 * \param [in] observer the observer
 * \param [in] slave1 index of the first slave
 * \param [in] valueReference1 the first value reference
 * \param [in] slave2 index of the second slave
 * \param [in] valueReference2 the second value reference
 * \param [in] fromStep the step number to start from
 * \param [in] nSamples the number of samples to read
 * \param [out] values1 the first series of observed values
 * \param [out] values2 the second series of observed values
 *
 * \returns
 *      The number of samples actually read, which may be smaller than `nSamples`.
 */
int64_t cosim_observer_slave_get_real_synchronized_series(
    cosim_observer* observer,
    cosim_slave_index slave1,
    cosim_value_reference valueReference1,
    cosim_slave_index slave2,
    cosim_value_reference valueReference2,
    cosim_step_number fromStep,
    size_t nSamples,
    double values1[],
    double values2[]);

/**
 * Retrieves the step numbers for a range given by a duration.
 *
 * Helper function which can be used in conjunction with `cosim_observer_slave_get_xxx_samples()`
 * when it is desired to retrieve the latest available samples given a certain duration.
 *
 * \note
 * It is assumed that `steps` has a length of 2.
 *
 * \param [in] observer the observer
 * \param [in] slave index of the slave
 * \param [in] duration the duration to get step numbers for
 * \param [out] steps the corresponding step numbers
 */
int cosim_observer_get_step_numbers_for_duration(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_duration duration,
    cosim_step_number steps[]);

/**
 * Retrieves the step numbers for a range given by two points in time.
 *
 * Helper function which can be used in conjunction with `cosim_observer_slave_get_xxx_samples()`
 * when it is desired to retrieve samples between two points in time.
 *
 * \note
 * It is assumed that `steps` has a length of 2.
 *
 * \param [in] observer the observer
 * \param [in] slave index of the simulator
 * \param [in] begin the start of the range
 * \param [in] end the end of the range
 * \param [out] steps the corresponding step numbers
 */
int cosim_observer_get_step_numbers(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_time_point begin,
    cosim_time_point end,
    cosim_step_number steps[]);

/**
 *  Connects one real output variable to one real input variable.
 *
 *  \param [in] execution
 *      The execution.
 *  \param [in] outputSlaveIndex
 *      The source slave.
 *  \param [in] outputValueReference
 *      The source variable.
 *  \param [in] inputSlaveIndex
 *      The destination slave.
 *  \param [in] inputValueReference
 *      The destination variable.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_connect_real_variables(
    cosim_execution* execution,
    cosim_slave_index outputSlaveIndex,
    cosim_value_reference outputValueReference,
    cosim_slave_index inputSlaveIndex,
    cosim_value_reference inputValueReference);

/**
 *  Connects one integer output variable to one integer input variable.
 *
 *  \param [in] execution
 *      The execution.
 *  \param [in] outputSlaveIndex
 *      The source slave.
 *  \param [in] outputValueReference
 *      The source variable.
 *  \param [in] inputSlaveIndex
 *      The destination slave.
 *  \param [in] inputValueReference
 *      The destination variable.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_connect_integer_variables(
    cosim_execution* execution,
    cosim_slave_index outputSlaveIndex,
    cosim_value_reference outputValueReference,
    cosim_slave_index inputSlaveIndex,
    cosim_value_reference inputValueReference);


/// Creates an observer which stores the last observed value for all variables.
cosim_observer* cosim_last_value_observer_create();

/**
 * Creates an observer which logs variable values to file in csv format.
 *
 * @param logDir
 *      The directory where log files will be created.
 * \returns
 *      The created observer.
 */
cosim_observer* cosim_file_observer_create(const char* logDir);

/**
 * Creates an observer which logs variable values to file in csv format. Variables to be logged
 * are specified in the supplied log config xml file.
 *
 * @param logDir
 *      The directory where log files will be created.
 * @param logConfigXml
 *      The path to the provided config xml file.
 * \returns
 *      The created observer.
 */
cosim_observer* cosim_file_observer_create_from_cfg(const char* logDir, const char* logConfigXml);

/**
 * Creates an observer which buffers variable values in memory. The buffer size is set to keep 10000 variable values in memory.
 *
 * To start observing a variable, `cosim_observer_start_observing()` must be called.
 */
cosim_observer* cosim_time_series_observer_create();

/**
 * Creates an observer which buffers up to `bufferSize` variable values in memory.
 *
 * To start observing a variable, `cosim_observer_start_observing()` must be called.
 */
cosim_observer* cosim_buffered_time_series_observer_create(size_t bufferSize);

/// Start observing a variable with a `time_series_observer`.
int cosim_observer_start_observing(cosim_observer* observer, cosim_slave_index slave, cosim_variable_type type, cosim_value_reference reference);

/// Stop observing a variable with a `time_series_observer`.
int cosim_observer_stop_observing(cosim_observer* observer, cosim_slave_index slave, cosim_variable_type type, cosim_value_reference reference);

/// Destroys an observer
int cosim_observer_destroy(cosim_observer* observer);


/**
 *  Add an observer to an execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param [in] observer
 *      A pointer to an observer, which may not be null. The observer may
 *      not previously have been added to any execution.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_add_observer(
    cosim_execution* execution,
    cosim_observer* observer);

/// Creates a manipulator for overriding variable values
cosim_manipulator* cosim_override_manipulator_create();

/**
 *  Add a manipulator to an execution.
 *
 *  \param [in] execution
 *      The execution.
 *  \param [in] manipulator
 *      A pointer to a manipulator, which may not be null. The manipulator may
 *      not previously have been added to any execution.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_execution_add_manipulator(
    cosim_execution* execution,
    cosim_manipulator* manipulator);

/// Destroys a manipulator
int cosim_manipulator_destroy(cosim_manipulator* manipulator);

/// Creates a manipulator for running scenarios.
cosim_manipulator* cosim_scenario_manager_create();

/// Loads and executes a scenario from file.
int cosim_execution_load_scenario(
    cosim_execution* execution,
    cosim_manipulator* manipulator,
    const char* scenarioFile);

/// Checks if a scenario is running
int cosim_scenario_is_running(cosim_manipulator* manipulator);

/// Aborts the execution of a running scenario
int cosim_scenario_abort(cosim_manipulator* manipulator);

/**
 * Retrieves a list of the currently modified variables in the simulation.
 *
 *  \param [in] execution
 *      The execution.
 *  \param [out] ids
 *      A list of cosim_variable_id structs to contain the variable information.
 *  \param [in] numVariables
 *      The length of the `ids` array.
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_get_modified_variables(cosim_execution* execution, cosim_variable_id ids[], size_t numVariables);


/// Severity levels for log messages.
typedef enum
{
    COSIM_LOG_SEVERITY_TRACE,
    COSIM_LOG_SEVERITY_DEBUG,
    COSIM_LOG_SEVERITY_INFO,
    COSIM_LOG_SEVERITY_WARNING,
    COSIM_LOG_SEVERITY_ERROR,
    COSIM_LOG_SEVERITY_FATAL
} cosim_log_severity_level;


/**
 *  Configures simple console logging.
 *
 *  Note that the library may produce log messages before this function is
 *  called, but then it uses the default or existing settings of the underlying
 *  logging framework (Boost.Log).
 *
 *  \returns
 *      0 on success and -1 on error.
 */
int cosim_log_setup_simple_console_logging();


/**
 *  Installs a global severity level filter for log messages.
 *
 *  This function sets up a log message filter which ensures that only messages
 *  whose severity level is at least `level` will be printed.
 *
 *  \param [in] level
 *      The minimum visible severity level.
 */
void cosim_log_set_output_level(cosim_log_severity_level level);


/// Software version
typedef struct
{
    int major;
    int minor;
    int patch;
} cosim_version;


/// Returns the version of the wrapped libcosim C++ library.
cosim_version cosim_libcosim_version();

/// Returns the libcosimc version.
cosim_version cosim_libcosimc_version();


#ifdef __cplusplus
} // extern(C)
#endif

#endif // header guard
