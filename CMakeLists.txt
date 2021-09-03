cmake_minimum_required(VERSION 3.13)
file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" projectVersion)
project("libcosimc"
    VERSION "${projectVersion}"
    DESCRIPTION "C wrapper for the libcosim C++ library"
)

# To enable verbose when needed
set(CMAKE_VERBOSE_MAKEFILE OFF)

# ==============================================================================
# Build settings
# ==============================================================================

option(BUILD_SHARED_LIBS "Build shared libraries instead of static libraries" ON)
option(LIBCOSIMC_TREAT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(LIBCOSIMC_BUILD_TESTS "Build test suite" ON)
option(LIBCOSIMC_STANDALONE_INSTALLATION "Whether to build for a standalone installation (Linux only; sets a relative RPATH)" OFF)
option(LIBCOSIMC_USING_CONAN "Whether Conan is used for package management" OFF)


# ==============================================================================
# Global internal configuration
# ==============================================================================

# Our custom CMake scripts go in the cmake/ subdirectory.
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")

# Use a common output directory for all targets.
# The main purpose of this is to ensure that the DLLs and test executables
# end up in the same directory on Windows, so that the OS finds the former
# when running the latter.
if(CMAKE_CONFIGURATION_TYPES)
    set(configTypes ${CMAKE_CONFIGURATION_TYPES})
else()
    set(configTypes Debug Release RelWithDebInfo MinSizeRel)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
endif()
foreach(c IN LISTS configTypes)
    string(TOLOWER "${c}" configL)
    string(TOUPPER "${c}" configU)
    set(outputDir "${CMAKE_BINARY_DIR}/output/${configL}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configU} "${outputDir}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configU} "${outputDir}/lib")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_${configU}     "${outputDir}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configU} "${outputDir}/bin")
endforeach()

# Use the highest warning levels and treat all warnings as errors,
# but ignore a few selected warnings.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-Wall" "-Wextra" "-Wpedantic")
    add_compile_options("-Wno-parentheses")
    if(LIBCOSIMC_TREAT_WARNINGS_AS_ERRORS)
        add_compile_options("-Werror")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options("/W4")
    if(CMAKE_GENERATOR MATCHES "NMake Makefiles.*")
        string(REPLACE "/W3" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        string(REPLACE "/W3" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
    endif()
    add_compile_options("/wd4996")
    if(LIBCOSIMC_TREAT_WARNINGS_AS_ERRORS)
        add_compile_options("/WX")
    endif()
    if(LIBCOSIMC_USING_CONAN)
        # C4251: 'identifier' : class 'type' needs to have dll-interface to be
        #        used by clients of class 'type2'
        #
        # This could be a problem if a dependent DLL is compiled against a
        # different version or variant of the C++ runtime than the one we
        # use.  An example would be if Boost was built in release mode and
        # we are compiling in release mode. However, when we use Conan, this
        # should not be a problem, so we disable the warning.
        add_compile_options("/wd4251")
    endif()
    add_definitions("-D_SCL_SECURE_NO_WARNINGS" "-D_CRT_SECURE_NO_WARNINGS")
endif()

# Automatically export all symbols in Windows DLLs.
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Organise projects in folders in IDEs that support it
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Prepare for export and installation.
set(LIBCOSIMC_HEADER_INSTALL_DIR "include")
if(WIN32)
    set(LIBCOSIMC_CMAKE_INSTALL_DIR "cmake")
    set(LIBCOSIMC_DOC_INSTALL_DIR   "doc")
else()
    set(LIBCOSIMC_CMAKE_INSTALL_DIR "share/${PROJECT_NAME}/cmake")
    set(LIBCOSIMC_DOC_INSTALL_DIR   "share/doc/${PROJECT_NAME}")
endif()
set(LIBCOSIMC_INSTALL_DESTINATIONS
    ARCHIVE DESTINATION "lib"
    LIBRARY DESTINATION "lib"
    RUNTIME DESTINATION "bin"
    INCLUDES DESTINATION "${LIBCOSIMC_HEADER_INSTALL_DIR}")
set(LIBCOSIMC_EXPORT_TARGET "${PROJECT_NAME}-targets")

# ==============================================================================
# Dependencies
# ==============================================================================

if(LIBCOSIMC_USING_CONAN)
    if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
        include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
        conan_basic_setup(NO_OUTPUT_DIRS)
    else()
        message(FATAL_ERROR "The file conanbuildinfo.cmake doesn't exist, you have to run conan install first")
    endif()
endif()

find_package(libcosim REQUIRED)

# ==============================================================================
# Targets
# ==============================================================================

set(generatedSources
    "lib_info.cpp"
)
set(generatedSourcesFull)
foreach(src IN LISTS generatedSources)
    set(tgt "${CMAKE_CURRENT_BINARY_DIR}/${src}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/src/${src}.in"
        "${tgt}"
        @ONLY
    )
    list(APPEND generatedSourcesFull "${tgt}")
endforeach()

add_library(cosimc "include/cosim.h" "src/cosim.cpp" ${generatedSourcesFull})

target_compile_features(cosimc PRIVATE "cxx_std_17")
target_include_directories(cosimc PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(cosimc PUBLIC libcosim::cosim stdc++)
else()
    target_link_libraries(cosimc PUBLIC libcosim::cosim)
endif()

if(WIN32 AND NOT BUILD_SHARED_LIBS)
    set_target_properties(cosimc PROPERTIES OUTPUT_NAME "libcosimc")
endif()
if(LIBCOSIMC_STANDALONE_INSTALLATION)
    set_target_properties(cosimc PROPERTIES INSTALL_RPATH "\$ORIGIN")
endif()

install(
        TARGETS cosimc
        EXPORT "${LIBCOSIMC_EXPORT_TARGET}"
        ${LIBCOSIMC_INSTALL_DESTINATIONS}
)
install(DIRECTORY "include/" DESTINATION "${LIBCOSIMC_HEADER_INSTALL_DIR}")

# ==============================================================================
# API documentation
# ==============================================================================

find_package(Doxygen)
if(DOXYGEN_FOUND)
    message("Found Doxygen, API documentation will be built.")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/doc")
    set(DOXYGEN_GENERATE_LATEX "NO")
    set(DOXYGEN_RECURSIVE "YES")
    set(DOXYGEN_SORT_BY_SCOPE_NAME "YES")
    set(DOXYGEN_STRIP_FROM_INC_PATH "${CMAKE_SOURCE_DIR}/include")
    set(DOXYGEN_STRIP_FROM_PATH "${CMAKE_SOURCE_DIR}/include")
    set(DOXYGEN_JAVADOC_AUTOBRIEF "YES")
    set(DOXYGEN_QT_AUTOBRIEF "YES")
    set(DOXYGEN_MULTILINE_CPP_IS_BRIEF "YES")
    set(doxygenInputs "${CMAKE_SOURCE_DIR}/include")
    doxygen_add_docs(doc ${doxygenInputs})
    add_custom_target(
        install-doc
        "${CMAKE_COMMAND}" "-E" "copy_directory"
            "${DOXYGEN_OUTPUT_DIRECTORY}"
            "${CMAKE_INSTALL_PREFIX}/${LIBCOSIMC_DOC_INSTALL_DIR}/"
        DEPENDS doc
    )
else()
    message(WARNING "API documentation will not be built since Doxygen was not found.")
endif()

# ==============================================================================
# Tests
# ==============================================================================

if(LIBCOSIMC_BUILD_TESTS)
    enable_testing()

    set(tests
            "connections_test"
            "execution_from_osp_config_test"
            "execution_from_ssp_custom_algo_test"
            "execution_from_ssp_test"
            "inital_values_test"
            "load_config_and_teardown_test"
            "multiple_fmus_execution_test"
            "observer_can_buffer_samples"
            "observer_initial_samples_test"
            "observer_multiple_slaves_test"
            "simulation_error_handling_test"
            "single_fmu_execution_test"
            "time_series_observer_test"
            "variable_metadata_test"
            )

    foreach(testName IN LISTS tests)
        add_executable("${testName}" "tests/${testName}.c")
        target_link_libraries("${testName}" PRIVATE cosimc)
        add_test(NAME "${testName}" COMMAND "${testName}")
        set_property(
            TEST "${testName}"
            PROPERTY ENVIRONMENT "TEST_DATA_DIR=${CMAKE_SOURCE_DIR}/tests/data"
        )
    endforeach()
endif()

# ==============================================================================
# Exports and remaining installation
# ==============================================================================

install(
    FILES "README.md" "LICENSE"
    DESTINATION "${LIBCOSIMC_DOC_INSTALL_DIR}"
)
install(
    EXPORT "${LIBCOSIMC_EXPORT_TARGET}"
    DESTINATION "${LIBCOSIMC_CMAKE_INSTALL_DIR}"
    NAMESPACE "${PROJECT_NAME}::"
)

include(CMakePackageConfigHelpers)

# Generate and install package-config file.
set(configFile "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config.cmake")
set(targetsFile "${LIBCOSIMC_CMAKE_INSTALL_DIR}/${LIBCOSIMC_EXPORT_TARGET}.cmake")
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/project-config.cmake.in"
    "${configFile}"
    INSTALL_DESTINATION "${LIBCOSIMC_CMAKE_INSTALL_DIR}"
    PATH_VARS targetsFile
)
install(FILES "${configFile}" DESTINATION "${LIBCOSIMC_CMAKE_INSTALL_DIR}")

# Generate and install package-version file
set(versionFile "${CMAKE_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake")
write_basic_package_version_file(
    "${versionFile}"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY "SameMajorVersion")
install(FILES "${versionFile}" DESTINATION "${LIBCOSIMC_CMAKE_INSTALL_DIR}")
