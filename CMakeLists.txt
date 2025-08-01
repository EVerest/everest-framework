cmake_minimum_required(VERSION 3.14)

project(everest-framework
    VERSION 0.22.3
    DESCRIPTION "The open operating system for e-mobility charging stations"
    LANGUAGES CXX C
)

find_package(everest-cmake 0.5 REQUIRED
    PATHS ../everest-cmake
)

# options
option(${PROJECT_NAME}_BUILD_TESTING "Build unit tests, used if included as dependency" OFF)
option(BUILD_TESTING "Build unit tests, used if standalone project" OFF)
option(FRAMEWORK_INSTALL "Install the library (shared data might be installed anyway)" ${EVC_MAIN_PROJECT})
option(CMAKE_RUN_CLANG_TIDY "Run clang-tidy" OFF)
option(EVEREST_ENABLE_JS_SUPPORT "Enable everestjs for JavaScript modules" OFF)
option(EVEREST_ENABLE_PY_SUPPORT "Enable everestpy for Python modules" ON)
option(EVEREST_ENABLE_RS_SUPPORT "Enable everestrs for Rust modules" OFF)
option(EVEREST_ENABLE_ADMIN_PANEL_BACKEND "Enable everest admin panel backend" ON)
option(EVEREST_INSTALL_ADMIN_PANEL "Download and install everest admin panel" ON)
ev_setup_cmake_variables_python_wheel()
option(${PROJECT_NAME}_USE_PYTHON_VENV "Use python venv for pip install targets" OFF)
set(${PROJECT_NAME}_PYTHON_VENV_PATH "${CMAKE_BINARY_DIR}/venv" CACHE PATH "Path to python venv")
ev_setup_python_executable(
    USE_PYTHON_VENV ${${PROJECT_NAME}_USE_PYTHON_VENV}
    PYTHON_VENV_PATH ${${PROJECT_NAME}_PYTHON_VENV_PATH}
)

if((${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME} OR ${PROJECT_NAME}_BUILD_TESTING) AND BUILD_TESTING)
    set(EVEREST_FRAMEWORK_BUILD_TESTING ON)
    # this policy allows us to link gcov to targets defined in other directories
    if(POLICY CMP0079)
        set(CMAKE_POLICY_DEFAULT_CMP0079 NEW)
    endif()
endif()

# make own cmake modules available
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# dependencies
find_package(Boost
    COMPONENTS
        program_options
        system
        thread
    REQUIRED
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(libcap
    REQUIRED
    IMPORTED_TARGET
    libcap
)

# Make sure stdc++fs is linked for GCC < 9
if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    set(STD_FILESYSTEM_COMPAT_LIB "stdc++fs")
else()
    set(STD_FILESYSTEM_COMPAT_LIB "")
endif()

if(NOT DISABLE_EDM)
    evc_setup_edm()

    # In EDM mode, we can't install exports (because the dependencies usually do not install their exports)
    set(FRAMEWORK_INSTALL OFF)

    if (EVEREST_ENABLE_ADMIN_PANEL_BACKEND)
        # FIXME (aw): libwebsockets/ninja clean doesn't delete recursivly ..
        set_property(
            TARGET websockets_shared
            APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES "${libwebsockets_BINARY_DIR}/include/libwebsockets"
        )

        # FIXME (aw): libwebsockets enum-int-mismatch FIX
        # see https://github.com/warmcat/libwebsockets/pull/2824
        if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 13.0)
            target_compile_options(websockets_shared PRIVATE -Wno-error=enum-int-mismatch)
        endif()
    endif()
    set_property(TARGET nlohmann_json_schema_validator PROPERTY POSITION_INDEPENDENT_CODE ON)

    # FIXME (aw): add catch2's cmake folder
    if (BUILD_TESTING)
        list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/contrib")
    endif()

    include(${everest-sqlite_SOURCE_DIR}/cmake/CollectMigrationFiles.cmake)
else()
    find_package(date REQUIRED)
    find_package(ryml REQUIRED)
    find_package(nlohmann_json REQUIRED)
    find_package(nlohmann_json_schema_validator REQUIRED)
    find_package(fmt REQUIRED)
    find_package(everest-log REQUIRED)
    find_package(everest-sqlite REQUIRED)

    if (EVEREST_ENABLE_ADMIN_PANEL_BACKEND)
        find_package(libwebsockets REQUIRED)
    endif()

    if (BUILD_TESTING)
        find_package(Catch2 REQUIRED)
    endif()

    include(find-mqttc)
endif()

set(EVEREST_FRAMEWORK_GENERATED_INC_DIR ${PROJECT_BINARY_DIR}/generated)
configure_file(
    include/compile_time_settings.hpp.in
    ${EVEREST_FRAMEWORK_GENERATED_INC_DIR}/everest/compile_time_settings.hpp
)

# library code
add_subdirectory(lib)

# executable code
add_subdirectory(src)

# auxillary files
add_subdirectory(schemas)

# everest javascript wrapper
if (EVEREST_ENABLE_JS_SUPPORT)
    add_subdirectory(everestjs)
endif()

# everest python wrapper
if (EVEREST_ENABLE_PY_SUPPORT)
    add_subdirectory(everestpy)
endif()

if (EVEREST_ENABLE_RS_SUPPORT)
  add_subdirectory(everestrs)
endif()

# FIXME (aw): should this be installed or not?  Right now it is needed for the
#             current packaging approach
install(TARGETS framework
    EXPORT framework-targets
    LIBRARY
)

# packaging
if (FRAMEWORK_INSTALL)
    install(
        DIRECTORY include/
        DESTINATION include/everest
    )

    install(
        FILES ${EVEREST_FRAMEWORK_GENERATED_INC_DIR}/everest/compile_time_settings.hpp
        DESTINATION include/everest
    )

    evc_setup_package(
        NAME everest-framework
        EXPORT framework-targets
        NAMESPACE everest
        ADDITIONAL_CONTENT
            "find_dependency(nlohmann_json)"
            "find_dependency(nlohmann_json_schema_validator)"
            "find_dependency(everest-timer)"
            "find_dependency(everest-log)"
            "find_dependency(everest-sqlite)"
            "find_dependency(fmt)"
            "find_dependency(date)"
            "set(EVEREST_SCHEMA_DIR \"@PACKAGE_EVEREST_SCHEMA_DIR@\")"
        PATH_VARS
            EVEREST_SCHEMA_DIR "${CMAKE_INSTALL_DATADIR}/everest/schemas"
    )
endif ()

# testing
if(EVEREST_FRAMEWORK_BUILD_TESTING)
    include(CTest)
    add_subdirectory(tests)
else()
    message(STATUS "Not running unit tests")
endif()

# configure clang-tidy if requested
if(CMAKE_RUN_CLANG_TIDY)
    message("Enabling clang-tidy")
    set(CMAKE_CXX_CLANG_TIDY clang-tidy)
endif()

# build doxygen documentation if doxygen is available
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set( DOXYGEN_OUTPUT_DIRECTORY dist/docs )
    doxygen_add_docs(doxygen-${PROJECT_NAME} everest.js include lib src)
else()
    message("Doxygen is needed to generate documentation")
endif()
