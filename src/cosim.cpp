/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#if defined(_WIN32) && !defined(NOMINMAX)
#    define NOMINMAX
#endif

#include <cosim.h>
#include <cosim/algorithm.hpp>
#include <cosim/exception.hpp>
#include <cosim/execution.hpp>
#include <cosim/fmi/fmu.hpp>
#include <cosim/fmi/importer.hpp>
#include <cosim/log/simple.hpp>
#include <cosim/manipulator.hpp>
#include <cosim/model_description.hpp>
#include <cosim/observer.hpp>
#include <cosim/orchestration.hpp>
#include <cosim/osp_config_parser.hpp>
#include <cosim/ssp/ssp_loader.hpp>
#include <cosim/time.hpp>

#include <boost/fiber/future.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

namespace
{
constexpr int success = 0;
constexpr int failure = -1;

cosim_errc cpp_to_c_error_code(std::error_code ec)
{
    if (ec == cosim::errc::bad_file)
        return COSIM_ERRC_BAD_FILE;
    else if (ec == cosim::errc::unsupported_feature)
        return COSIM_ERRC_UNSUPPORTED_FEATURE;
    else if (ec == cosim::errc::dl_load_error)
        return COSIM_ERRC_DL_LOAD_ERROR;
    else if (ec == cosim::errc::model_error)
        return COSIM_ERRC_MODEL_ERROR;
    else if (ec == cosim::errc::simulation_error)
        return COSIM_ERRC_SIMULATION_ERROR;
    else if (ec == cosim::errc::zip_error)
        return COSIM_ERRC_ZIP_ERROR;
    else if (ec.category() == std::generic_category()) {
        errno = ec.value();
        return COSIM_ERRC_ERRNO;
    } else {
        return COSIM_ERRC_UNSPECIFIED;
    }
}

// These hold information about the last reported error.
// They should only be set through `set_last_error()` and
// `handle_current_exception()`.
thread_local cosim_errc g_lastErrorCode;
thread_local std::string g_lastErrorMessage;

// Sets the last error code and message directly.
void set_last_error(cosim_errc ec, std::string message)
{
    g_lastErrorCode = ec;
    g_lastErrorMessage = std::move(message);
}

// Sets the last error code based on an `std::error_code`.
// If a message is specified it is used, otherwise the default
// message from `ec` is used.
void set_last_error(std::error_code ec, std::optional<std::string> message)
{
    set_last_error(
        cpp_to_c_error_code(ec),
        std::move(message).value_or(ec.message()));
}

// To be called from an exception handler (`catch` block).  Stores
// information about the current exception as an error code and message.
void handle_current_exception()
{
    try {
        throw;
    } catch (const cosim::error& e) {
        set_last_error(e.code(), e.what());
    } catch (const std::system_error& e) {
        set_last_error(e.code(), e.what());
    } catch (const std::invalid_argument& e) {
        set_last_error(COSIM_ERRC_INVALID_ARGUMENT, e.what());
    } catch (const std::out_of_range& e) {
        set_last_error(COSIM_ERRC_OUT_OF_RANGE, e.what());
    } catch (const std::exception& e) {
        set_last_error(COSIM_ERRC_UNSPECIFIED, e.what());
    } catch (...) {
        set_last_error(
            COSIM_ERRC_UNSPECIFIED,
            "An exception of unknown type was thrown");
    }
}

constexpr cosim_time_point to_integer_time_point(cosim::time_point t)
{
    return t.time_since_epoch().count();
}

constexpr cosim::duration to_duration(cosim_duration nanos)
{
    return std::chrono::duration<cosim::detail::clock::rep, cosim::detail::clock::period>(nanos);
}

constexpr cosim::time_point to_time_point(cosim_time_point nanos)
{
    return cosim::time_point(to_duration(nanos));
}

// A wrapper for `std::strncpy()` which ensures that `dest` is always
// null terminated.
void safe_strncpy(char* dest, const char* src, std::size_t count)
{
    std::strncpy(dest, src, count - 1);
    dest[count - 1] = '\0';
}

} // namespace


cosim_errc cosim_last_error_code()
{
    return g_lastErrorCode;
}

const char* cosim_last_error_message()
{
    return g_lastErrorMessage.c_str();
}

struct cosim_execution_s
{
    std::unique_ptr<cosim::execution> cpp_execution;
    cosim::entity_index_maps entity_maps;
    std::thread t;
    boost::fibers::future<bool> simulate_result;
    std::exception_ptr simulate_exception_ptr;
    std::atomic<cosim_execution_state> state;
    int error_code;
};

cosim_execution* cosim_execution_create(cosim_time_point startTime, cosim_duration stepSize)
{
    try {
        // No exceptions are possible right now, so try...catch and unique_ptr
        // are strictly unnecessary, but this will change soon enough.
        auto execution = std::make_unique<cosim_execution>();

        execution->cpp_execution = std::make_unique<cosim::execution>(
            to_time_point(startTime),
            std::make_unique<cosim::fixed_step_algorithm>(to_duration(stepSize)));
        execution->error_code = COSIM_ERRC_SUCCESS;
        execution->state = COSIM_EXECUTION_STOPPED;

        return execution.release();
    } catch (...) {
        handle_current_exception();
        return nullptr;
    }
}

cosim_execution* cosim_osp_config_execution_create(
    const char* configPath,
    bool startTimeDefined,
    cosim_time_point startTime)
{
    try {
        auto execution = std::make_unique<cosim_execution>();

        auto resolver = cosim::default_model_uri_resolver();
        const auto config = cosim::load_osp_config(configPath, *resolver);

        execution->cpp_execution = std::make_unique<cosim::execution>(
            startTimeDefined ? to_time_point(startTime) : config.start_time,
            std::make_shared<cosim::fixed_step_algorithm>(config.step_size));
        execution->entity_maps = cosim::inject_system_structure(
            *execution->cpp_execution,
            config.system_structure,
            config.initial_values);

        return execution.release();
    } catch (...) {
        handle_current_exception();
        return nullptr;
    }
}


cosim_execution* cosim_ssp_execution_create(
    const char* sspDir,
    bool startTimeDefined,
    cosim_time_point startTime)
{
    try {
        auto execution = std::make_unique<cosim_execution>();

        cosim::ssp_loader loader;
        const auto config = loader.load(sspDir);

        execution->cpp_execution = std::make_unique<cosim::execution>(
            startTimeDefined ? to_time_point(startTime) : config.start_time,
            config.algorithm);
        execution->entity_maps = cosim::inject_system_structure(
            *execution->cpp_execution,
            config.system_structure,
            config.parameter_sets.at(""));

        return execution.release();
    } catch (...) {
        handle_current_exception();
        return nullptr;
    }
}

cosim_execution* cosim_ssp_fixed_step_execution_create(
    const char* sspDir,
    bool startTimeDefined,
    cosim_time_point startTime,
    cosim_duration stepSize)
{
    try {
        auto execution = std::make_unique<cosim_execution>();

        cosim::ssp_loader loader;
        const auto config = loader.load(sspDir);

        execution->cpp_execution = std::make_unique<cosim::execution>(
            startTimeDefined ? to_time_point(startTime) : config.start_time,
            std::make_unique<cosim::fixed_step_algorithm>(to_duration(stepSize)));
        execution->entity_maps = cosim::inject_system_structure(
            *execution->cpp_execution,
            config.system_structure,
            config.parameter_sets.at(""));

        return execution.release();
    } catch (...) {
        handle_current_exception();
        return nullptr;
    }
}

int cosim_execution_destroy(cosim_execution* execution)
{
    try {
        if (!execution) return success;
        const auto owned = std::unique_ptr<cosim_execution>(execution);
        cosim_execution_stop(execution);
        return success;
    } catch (...) {
        execution->state = COSIM_EXECUTION_ERROR;
        execution->error_code = COSIM_ERRC_UNSPECIFIED;
        handle_current_exception();
        return failure;
    }
}

size_t cosim_execution_get_num_slaves(cosim_execution* execution)
{
    return execution->entity_maps.simulators.size();
}

int cosim_execution_get_slave_infos(cosim_execution* execution, cosim_slave_info infos[], size_t numSlaves)
{
    try {
        auto ids = execution->entity_maps.simulators;
        size_t slave = 0;
        for (const auto& [name, index] : ids) {
            safe_strncpy(infos[slave].name, name.c_str(), SLAVE_NAME_MAX_SIZE);
            infos[slave].index = index;
            if (++slave >= numSlaves) {
                break;
            }
        }
        return success;
    } catch (...) {
        execution->state = COSIM_EXECUTION_ERROR;
        execution->error_code = COSIM_ERRC_UNSPECIFIED;
        handle_current_exception();
        return failure;
    }
}

int cosim_slave_get_num_variables(cosim_execution* execution, cosim_slave_index slave)
{
    try {
        return static_cast<int>(execution
                                    ->cpp_execution
                                    ->get_simulator(slave)
                                    ->model_description()
                                    .variables
                                    .size());
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_get_num_modified_variables(cosim_execution* execution)
{
    return static_cast<int>(execution->cpp_execution->get_modified_variables().size());
}

cosim_variable_variability to_variable_variability(const cosim::variable_variability& vv)
{
    switch (vv) {
        case cosim::variable_variability::constant:
            return COSIM_VARIABLE_VARIABILITY_CONSTANT;
        case cosim::variable_variability::continuous:
            return COSIM_VARIABLE_VARIABILITY_CONTINUOUS;
        case cosim::variable_variability::discrete:
            return COSIM_VARIABLE_VARIABILITY_DISCRETE;
        case cosim::variable_variability::fixed:
            return COSIM_VARIABLE_VARIABILITY_FIXED;
        case cosim::variable_variability::tunable:
            return COSIM_VARIABLE_VARIABILITY_TUNABLE;
        default:
            throw std::invalid_argument("Invalid variable variability!");
    }
}

cosim_variable_causality to_variable_causality(const cosim::variable_causality& vc)
{
    switch (vc) {
        case cosim::variable_causality::input:
            return COSIM_VARIABLE_CAUSALITY_INPUT;
        case cosim::variable_causality::output:
            return COSIM_VARIABLE_CAUSALITY_OUTPUT;
        case cosim::variable_causality::parameter:
            return COSIM_VARIABLE_CAUSALITY_PARAMETER;
        case cosim::variable_causality::calculated_parameter:
            return COSIM_VARIABLE_CAUSALITY_CALCULATEDPARAMETER;
        case cosim::variable_causality::local:
            return COSIM_VARIABLE_CAUSALITY_LOCAL;
        default:
            throw std::invalid_argument("Invalid variable causality!");
    }
}

cosim_variable_type to_c_variable_type(const cosim::variable_type& vt)
{
    switch (vt) {
        case cosim::variable_type::real:
            return COSIM_VARIABLE_TYPE_REAL;
        case cosim::variable_type::integer:
            return COSIM_VARIABLE_TYPE_INTEGER;
        case cosim::variable_type::boolean:
            return COSIM_VARIABLE_TYPE_BOOLEAN;
        case cosim::variable_type::string:
            return COSIM_VARIABLE_TYPE_STRING;
        default:
            throw std::invalid_argument("Invalid variable type!");
    }
}

void translate_variable_description(const cosim::variable_description& vd, cosim_variable_description& cvd)
{
    safe_strncpy(cvd.name, vd.name.c_str(), SLAVE_NAME_MAX_SIZE);
    cvd.reference = vd.reference;
    cvd.type = to_c_variable_type(vd.type);
    cvd.causality = to_variable_causality(vd.causality);
    cvd.variability = to_variable_variability(vd.variability);
}

int cosim_slave_get_variables(cosim_execution* execution, cosim_slave_index slave, cosim_variable_description variables[], size_t numVariables)
{
    try {
        const auto vars = execution
                              ->cpp_execution
                              ->get_simulator(slave)
                              ->model_description()
                              .variables;
        size_t var = 0;
        for (; var < std::min(numVariables, vars.size()); var++) {
            translate_variable_description(vars.at(var), variables[var]);
        }
        return static_cast<int>(var);
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

struct cosim_slave_s
{
    std::string address;
    std::string modelName;
    std::string instanceName;
    std::shared_ptr<cosim::slave> instance;
};

cosim_slave* cosim_local_slave_create(const char* fmuPath, const char* instanceName)
{
    try {
        const auto importer = cosim::fmi::importer::create();
        const auto fmu = importer->import(fmuPath);
        auto slave = std::make_unique<cosim_slave>();
        slave->modelName = fmu->model_description()->name;
        slave->instanceName = std::string(instanceName);
        slave->instance = fmu->instantiate_slave(slave->instanceName);
        // slave address not in use yet. Should be something else than a string.
        slave->address = "local";
        return slave.release();
    } catch (...) {
        handle_current_exception();
        return nullptr;
    }
}

int cosim_execution_set_real_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, double value)
{
    try {
        execution->cpp_execution->set_real_initial_value(slaveIndex, vr, value);
    } catch (...) {
        handle_current_exception();
        return failure;
    }
    return success;
}

int cosim_execution_set_integer_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, int value)
{
    try {
        execution->cpp_execution->set_integer_initial_value(slaveIndex, vr, value);
    } catch (...) {
        handle_current_exception();
        return failure;
    }
    return success;
}

int cosim_execution_set_boolean_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, bool value)
{
    try {
        execution->cpp_execution->set_boolean_initial_value(slaveIndex, vr, value);
    } catch (...) {
        handle_current_exception();
        return failure;
    }
    return success;
}

int cosim_execution_set_string_initial_value(cosim_execution* execution, cosim_slave_index slaveIndex, cosim_value_reference vr, char* value)
{
    try {
        execution->cpp_execution->set_string_initial_value(slaveIndex, vr, value);
    } catch (...) {
        handle_current_exception();
        return failure;
    }
    return success;
}

int cosim_local_slave_destroy(cosim_slave* slave)
{
    try {
        if (!slave) return success;
        const auto owned = std::unique_ptr<cosim_slave>(slave);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

cosim_slave_index cosim_execution_add_slave(
    cosim_execution* execution,
    cosim_slave* slave)
{
    try {
        auto index = execution->cpp_execution->add_slave(cosim::make_background_thread_slave(slave->instance), slave->instanceName);
        execution->entity_maps.simulators[slave->instanceName] = index;
        return index;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

void cosim_execution_step(cosim_execution* execution)
{
    execution->cpp_execution->step();
}

int cosim_execution_step(cosim_execution* execution, size_t numSteps)
{
    if (execution->cpp_execution->is_running()) {
        return success;
    } else {
        execution->state = COSIM_EXECUTION_RUNNING;
        for (size_t i = 0; i < numSteps; i++) {
            try {
                cosim_execution_step(execution);
            } catch (...) {
                handle_current_exception();
                execution->state = COSIM_EXECUTION_ERROR;
                return failure;
            }
        }
        execution->state = COSIM_EXECUTION_STOPPED;
        return success;
    }
}

int cosim_execution_simulate_until(cosim_execution* execution, cosim_time_point targetTime)
{
    if (execution->cpp_execution->is_running()) {
        set_last_error(COSIM_ERRC_ILLEGAL_STATE, "Function 'cosim_execution_simulate_until' may not be called while simulation is running!");
        return failure;
    } else {
        execution->state = COSIM_EXECUTION_RUNNING;
        try {
            const bool notStopped = execution->cpp_execution->simulate_until(to_time_point(targetTime)).get();
            execution->state = COSIM_EXECUTION_STOPPED;
            return notStopped;
        } catch (...) {
            handle_current_exception();
            execution->state = COSIM_EXECUTION_ERROR;
            return failure;
        }
    }
}

int cosim_execution_start(cosim_execution* execution)
{
    if (execution->t.joinable()) {
        return success;
    } else {
        try {
            execution->state = COSIM_EXECUTION_RUNNING;
            auto task = boost::fibers::packaged_task<bool()>([execution]() {
                return execution->cpp_execution->simulate_until(std::nullopt).get();
            });
            execution->simulate_result = task.get_future();
            execution->t = std::thread(std::move(task));
            return success;
        } catch (...) {
            handle_current_exception();
            execution->state = COSIM_EXECUTION_ERROR;
            return failure;
        }
    }
}

void execution_async_health_check(cosim_execution* execution)
{
    if (execution->simulate_result.valid()) {
        const auto status = execution->simulate_result.wait_for(std::chrono::duration<int64_t>());
        if (boost::fibers::future_status::ready == status) {
            if (auto ep = execution->simulate_result.get_exception_ptr()) {
                execution->simulate_exception_ptr = ep;
            }
        }
    }
    if (auto ep = execution->simulate_exception_ptr) {
        std::rethrow_exception(ep);
    }
}

int cosim_execution_stop(cosim_execution* execution)
{
    try {
        execution->cpp_execution->stop_simulation();
        if (execution->t.joinable()) {
            execution->simulate_result.get();
            execution->t.join();
        }
        execution->state = COSIM_EXECUTION_STOPPED;
        return success;
    } catch (...) {
        execution->t.join();
        handle_current_exception();
        execution->state = COSIM_EXECUTION_ERROR;
        return failure;
    }
}

int cosim_execution_get_status(cosim_execution* execution, cosim_execution_status* status)
{
    try {
        status->error_code = execution->error_code;
        status->state = execution->state;
        status->current_time = to_integer_time_point(execution->cpp_execution->current_time());
        status->real_time_factor = execution->cpp_execution->get_measured_real_time_factor();
        status->real_time_factor_target = execution->cpp_execution->get_real_time_factor_target();
        status->is_real_time_simulation = execution->cpp_execution->is_real_time_simulation() ? 1 : 0;
        execution_async_health_check(execution);
        return success;
    } catch (...) {
        handle_current_exception();
        execution->error_code = cosim_last_error_code();
        execution->state = COSIM_EXECUTION_ERROR;
        status->error_code = execution->error_code;
        status->state = execution->state;
        return failure;
    }
}

int cosim_execution_enable_real_time_simulation(cosim_execution* execution)
{
    try {
        execution->cpp_execution->enable_real_time_simulation();
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_execution_disable_real_time_simulation(cosim_execution* execution)
{
    try {
        execution->cpp_execution->disable_real_time_simulation();
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_execution_set_real_time_factor_target(cosim_execution* execution, double realTimeFactor)
{
    try {
        execution->cpp_execution->set_real_time_factor_target(realTimeFactor);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

struct cosim_observer_s
{
    std::shared_ptr<cosim::observer> cpp_observer;
};

int cosim_observer_destroy(cosim_observer* observer)
{
    try {
        if (!observer) return success;
        const auto owned = std::unique_ptr<cosim_observer>(observer);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int connect_variables(
    cosim_execution* execution,
    cosim::simulator_index outputSimulator,
    cosim::value_reference outputVariable,
    cosim::simulator_index inputSimulator,
    cosim::value_reference inputVariable,
    cosim::variable_type type)
{
    try {
        const auto outputId = cosim::variable_id{outputSimulator, type, outputVariable};
        const auto inputId = cosim::variable_id{inputSimulator, type, inputVariable};
        execution->cpp_execution->connect_variables(outputId, inputId);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_execution_connect_real_variables(
    cosim_execution* execution,
    cosim_slave_index outputSlaveIndex,
    cosim_value_reference outputValueReference,
    cosim_slave_index inputSlaveIndex,
    cosim_value_reference inputValueReference)
{
    return connect_variables(execution, outputSlaveIndex, outputValueReference, inputSlaveIndex, inputValueReference,
        cosim::variable_type::real);
}

int cosim_execution_connect_integer_variables(
    cosim_execution* execution,
    cosim_slave_index outputSlaveIndex,
    cosim_value_reference outputValueReference,
    cosim_slave_index inputSlaveIndex,
    cosim_value_reference inputValueReference)
{
    return connect_variables(execution, outputSlaveIndex, outputValueReference, inputSlaveIndex, inputValueReference,
        cosim::variable_type::integer);
}

int cosim_observer_slave_get_real(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    double values[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::last_value_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a last_value_observer.");
        }
        obs->get_real(slave, gsl::make_span(variables, nv), gsl::make_span(values, nv));
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_observer_slave_get_integer(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    int values[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::last_value_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a last_value_observer.");
        }
        obs->get_integer(slave, gsl::make_span(variables, nv), gsl::make_span(values, nv));
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_observer_slave_get_boolean(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    bool values[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::last_value_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a last_value_observer.");
        }
        obs->get_boolean(slave, gsl::make_span(variables, nv), gsl::make_span(values, nv));
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

// This holds string variable values.
// Must only be used with `cosim_observer_slave_get_string()`.
thread_local std::vector<std::string> g_stringVariableBuffer;

int cosim_observer_slave_get_string(
    cosim_observer* observer,
    cosim_slave_index slave,
    const cosim_value_reference variables[],
    size_t nv,
    const char* values[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::last_value_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a last_value_observer.");
        }
        g_stringVariableBuffer.clear();
        g_stringVariableBuffer.resize(nv);
        obs->get_string(slave, gsl::make_span(variables, nv), gsl::span<std::string>(g_stringVariableBuffer));
        for (size_t i = 0; i < nv; i++) {
            values[i] = g_stringVariableBuffer.at(i).c_str();
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int64_t cosim_observer_slave_get_real_samples(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_value_reference valueReference,
    cosim_step_number fromStep,
    size_t nSamples,
    double values[],
    cosim_step_number steps[],
    cosim_time_point times[])
{
    try {
        std::vector<cosim::time_point> timePoints(nSamples);
        const auto obs = std::dynamic_pointer_cast<cosim::time_series_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        size_t samplesRead = obs->get_real_samples(slave, valueReference, fromStep,
            gsl::make_span(values, nSamples),
            gsl::make_span(steps, nSamples), timePoints);
        for (size_t i = 0; i < samplesRead; ++i) {
            times[i] = to_integer_time_point(timePoints[i]);
        }
        return samplesRead;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int64_t cosim_observer_slave_get_real_synchronized_series(
    cosim_observer* observer,
    cosim_slave_index slave1,
    cosim_value_reference valueReference1,
    cosim_slave_index slave2,
    cosim_value_reference valueReference2,
    cosim_step_number fromStep,
    size_t nSamples,
    double values1[],
    double values2[])
{
    try {
        std::vector<cosim::time_point> timePoints(nSamples);
        const auto obs = std::dynamic_pointer_cast<cosim::time_series_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        size_t samplesRead = obs->get_synchronized_real_series(
            slave1,
            valueReference1,
            slave2,
            valueReference2,
            fromStep,
            gsl::make_span(values1, nSamples),
            gsl::make_span(values2, nSamples));
        return samplesRead;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int64_t cosim_observer_slave_get_integer_samples(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_value_reference valueReference,
    cosim_step_number fromStep,
    size_t nSamples,
    int values[],
    cosim_step_number steps[],
    cosim_time_point times[])
{
    try {
        std::vector<cosim::time_point> timePoints(nSamples);
        const auto obs = std::dynamic_pointer_cast<cosim::time_series_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        size_t samplesRead = obs->get_integer_samples(slave, valueReference, fromStep,
            gsl::make_span(values, nSamples),
            gsl::make_span(steps, nSamples), timePoints);
        for (size_t i = 0; i < samplesRead; ++i) {
            times[i] = to_integer_time_point(timePoints[i]);
        }
        return samplesRead;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_observer_get_step_numbers_for_duration(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_duration duration,
    cosim_step_number steps[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::time_series_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        obs->get_step_numbers(slave, to_duration(duration), gsl::make_span(steps, 2));
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_observer_get_step_numbers(
    cosim_observer* observer,
    cosim_slave_index slave,
    cosim_time_point begin,
    cosim_time_point end,
    cosim_step_number steps[])
{
    try {
        const auto obs = std::dynamic_pointer_cast<cosim::time_series_provider>(observer->cpp_observer);
        if (!obs) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        obs->get_step_numbers(slave, to_time_point(begin), to_time_point(end),
            gsl::make_span(steps, 2));
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

cosim_observer* cosim_last_value_observer_create()
{
    auto observer = std::make_unique<cosim_observer>();
    observer->cpp_observer = std::make_shared<cosim::last_value_observer>();
    return observer.release();
}

cosim_observer* cosim_file_observer_create(const char* logDir)
{
    auto observer = std::make_unique<cosim_observer>();
    auto logPath = boost::filesystem::path(logDir);
    observer->cpp_observer = std::make_shared<cosim::file_observer>(logPath);
    return observer.release();
}

cosim_observer* cosim_file_observer_create_from_cfg(const char* logDir, const char* cfgPath)
{
    auto observer = std::make_unique<cosim_observer>();
    auto boostLogDir = boost::filesystem::path(logDir);
    auto boostCfgPath = boost::filesystem::path(cfgPath);
    observer->cpp_observer = std::make_shared<cosim::file_observer>(boostLogDir, boostCfgPath);
    return observer.release();
}

cosim_observer* cosim_time_series_observer_create()
{
    auto observer = std::make_unique<cosim_observer>();
    observer->cpp_observer = std::make_shared<cosim::time_series_observer>();
    return observer.release();
}

cosim_observer* cosim_buffered_time_series_observer_create(size_t bufferSize)
{
    auto observer = std::make_unique<cosim_observer>();
    observer->cpp_observer = std::make_shared<cosim::time_series_observer>(bufferSize);
    return observer.release();
}

cosim::variable_type to_cpp_variable_type(cosim_variable_type type)
{
    switch (type) {
        case COSIM_VARIABLE_TYPE_REAL:
            return cosim::variable_type::real;
        case COSIM_VARIABLE_TYPE_INTEGER:
            return cosim::variable_type::integer;
        case COSIM_VARIABLE_TYPE_BOOLEAN:
            return cosim::variable_type::boolean;
        case COSIM_VARIABLE_TYPE_STRING:
            return cosim::variable_type::string;
        default:
            throw std::invalid_argument("Variable type not supported");
    }
}

int cosim_observer_start_observing(cosim_observer* observer, cosim_slave_index slave, cosim_variable_type type, cosim_value_reference reference)
{
    try {
        const auto timeSeriesObserver = std::dynamic_pointer_cast<cosim::time_series_observer>(observer->cpp_observer);
        if (!timeSeriesObserver) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        const auto variableId = cosim::variable_id{slave, to_cpp_variable_type(type), reference};
        timeSeriesObserver->start_observing(variableId);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_observer_stop_observing(cosim_observer* observer, cosim_slave_index slave, cosim_variable_type type, cosim_value_reference reference)
{
    try {
        const auto timeSeriesObserver = std::dynamic_pointer_cast<cosim::time_series_observer>(observer->cpp_observer);
        if (!timeSeriesObserver) {
            throw std::invalid_argument("Invalid observer! The provided observer must be a time_series_observer.");
        }
        const auto variableId = cosim::variable_id{slave, to_cpp_variable_type(type), reference};
        timeSeriesObserver->stop_observing(variableId);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_execution_add_observer(
    cosim_execution* execution,
    cosim_observer* observer)
{
    try {
        execution->cpp_execution->add_observer(observer->cpp_observer);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

struct cosim_manipulator_s
{
    std::shared_ptr<cosim::manipulator> cpp_manipulator;
};

cosim_manipulator* cosim_override_manipulator_create()
{
    auto manipulator = std::make_unique<cosim_manipulator>();
    manipulator->cpp_manipulator = std::make_shared<cosim::override_manipulator>();
    return manipulator.release();
}

int cosim_manipulator_destroy(cosim_manipulator* manipulator)
{
    try {
        if (!manipulator) return success;
        const auto owned = std::unique_ptr<cosim_manipulator>(manipulator);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_execution_add_manipulator(
    cosim_execution* execution,
    cosim_manipulator* manipulator)
{
    try {
        execution->cpp_execution->add_manipulator(manipulator->cpp_manipulator);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_manipulator_slave_set_real(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const double values[])
{
    try {
        const auto man = std::dynamic_pointer_cast<cosim::override_manipulator>(manipulator->cpp_manipulator);
        if (!man) {
            throw std::invalid_argument("Invalid manipulator!");
        }
        for (size_t i = 0; i < nv; i++) {
            man->override_real_variable(slaveIndex, variables[i], values[i]);
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_manipulator_slave_set_integer(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const int values[])
{
    try {
        const auto man = std::dynamic_pointer_cast<cosim::override_manipulator>(manipulator->cpp_manipulator);
        if (!man) {
            throw std::invalid_argument("Invalid manipulator!");
        }
        for (size_t i = 0; i < nv; i++) {
            man->override_integer_variable(slaveIndex, variables[i], values[i]);
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_manipulator_slave_set_boolean(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const bool values[])
{
    try {
        const auto man = std::dynamic_pointer_cast<cosim::override_manipulator>(manipulator->cpp_manipulator);
        if (!man) {
            throw std::invalid_argument("Invalid manipulator!");
        }
        for (size_t i = 0; i < nv; i++) {
            man->override_boolean_variable(slaveIndex, variables[i], values[i]);
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_manipulator_slave_set_string(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    const cosim_value_reference variables[],
    size_t nv,
    const char* values[])
{
    try {
        const auto man = std::dynamic_pointer_cast<cosim::override_manipulator>(manipulator->cpp_manipulator);
        if (!man) {
            throw std::invalid_argument("Invalid manipulator!");
        }
        for (size_t i = 0; i < nv; i++) {
            man->override_string_variable(slaveIndex, variables[i], values[i]);
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_manipulator_slave_reset(
    cosim_manipulator* manipulator,
    cosim_slave_index slaveIndex,
    cosim_variable_type type,
    const cosim_value_reference variables[],
    size_t nv)
{
    try {
        const auto man = std::dynamic_pointer_cast<cosim::override_manipulator>(manipulator->cpp_manipulator);
        if (!man) {
            throw std::invalid_argument("Invalid manipulator!");
        }
        cosim::variable_type vt = to_cpp_variable_type(type);
        for (size_t i = 0; i < nv; i++) {
            man->reset_variable(slaveIndex, vt, variables[i]);
        }
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

cosim_manipulator* cosim_scenario_manager_create()
{
    auto manipulator = std::make_unique<cosim_manipulator>();
    manipulator->cpp_manipulator = std::make_shared<cosim::scenario_manager>();
    return manipulator.release();
}

int cosim_execution_load_scenario(
    cosim_execution* execution,
    cosim_manipulator* manipulator,
    const char* scenarioFile)
{
    try {
        auto time = execution->cpp_execution->current_time();
        const auto manager = std::dynamic_pointer_cast<cosim::scenario_manager>(manipulator->cpp_manipulator);
        if (!manager) {
            throw std::invalid_argument("Invalid manipulator! The provided manipulator must be a scenario_manager.");
        }
        manager->load_scenario(scenarioFile, time);
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_scenario_is_running(cosim_manipulator* manipulator)
{
    try {
        const auto manager = std::dynamic_pointer_cast<cosim::scenario_manager>(manipulator->cpp_manipulator);
        if (!manager) {
            throw std::invalid_argument("Invalid manipulator! The provided manipulator must be a scenario_manager.");
        }
        return manager->is_scenario_running();
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_scenario_abort(cosim_manipulator* manipulator)
{
    try {
        const auto manager = std::dynamic_pointer_cast<cosim::scenario_manager>(manipulator->cpp_manipulator);
        if (!manager) {
            throw std::invalid_argument("Invalid manipulator! The provided manipulator must be a scenario_manager.");
        }
        manager->abort_scenario();
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}

int cosim_get_modified_variables(cosim_execution* execution, cosim_variable_id ids[], size_t numVariables)
{
    try {
        auto modified_vars = execution->cpp_execution->get_modified_variables();
        size_t counter = 0;

        if (!modified_vars.empty()) {
            for (; counter < std::min(numVariables, modified_vars.size()); counter++) {
                ids[counter].slave_index = modified_vars[counter].simulator;
                ids[counter].type = to_c_variable_type(modified_vars[counter].type);
                ids[counter].value_reference = modified_vars[counter].reference;
            }
        }

        return static_cast<int>(counter);
    } catch (...) {
        execution->state = COSIM_EXECUTION_ERROR;
        execution->error_code = COSIM_ERRC_UNSPECIFIED;
        handle_current_exception();
        return failure;
    }
}


int cosim_log_setup_simple_console_logging()
{
    try {
        cosim::log::setup_simple_console_logging();
        return success;
    } catch (...) {
        handle_current_exception();
        return failure;
    }
}


void cosim_log_set_output_level(cosim_log_severity_level level)
{
    switch (level) {
        case COSIM_LOG_SEVERITY_TRACE:
            cosim::log::set_global_output_level(cosim::log::trace);
            break;
        case COSIM_LOG_SEVERITY_DEBUG:
            cosim::log::set_global_output_level(cosim::log::debug);
            break;
        case COSIM_LOG_SEVERITY_INFO:
            cosim::log::set_global_output_level(cosim::log::info);
            break;
        case COSIM_LOG_SEVERITY_WARNING:
            cosim::log::set_global_output_level(cosim::log::warning);
            break;
        case COSIM_LOG_SEVERITY_ERROR:
            cosim::log::set_global_output_level(cosim::log::error);
            break;
        case COSIM_LOG_SEVERITY_FATAL:
            cosim::log::set_global_output_level(cosim::log::fatal);
            break;
        default:
            assert(false);
    }
}
