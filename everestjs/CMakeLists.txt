#cmake_policy(SET CMP0042 NEW)

add_library(everestjs SHARED everestjs.cpp conversions.cpp js_exec_ctx.cpp)
set_target_properties(everestjs PROPERTIES PREFIX "" SUFFIX ".node")

target_link_libraries(everestjs
        PRIVATE everest everest::log
)

find_program(
        NODE
        node
        REQUIRED
)

# check node API version based on https://nodejs.org/api/n-api.html#node-api-version-matrix
execute_process(COMMAND ${NODE} --version
        OUTPUT_VARIABLE NODE_VERSION
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_VERSION "${NODE_VERSION}")
string(REPLACE "v" "" NODE_VERSION "${NODE_VERSION}")
if(     ("${NODE_VERSION}" VERSION_GREATER_EQUAL "16")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "15.12")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "12.22"))
        set(NODE_API_VERSION 8)
elseif(     ("${NODE_VERSION}" VERSION_GREATER_EQUAL "15")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "14.12")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "12.19")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "10.23"))
        set(NODE_API_VERSION 7)
elseif(     ("${NODE_VERSION}" VERSION_GREATER_EQUAL "14")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "12.17")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "10.20"))
        set(NODE_API_VERSION 6)
elseif(     ("${NODE_VERSION}" VERSION_GREATER_EQUAL "13")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "12.11")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "10.17"))
        set(NODE_API_VERSION 5)
elseif(     ("${NODE_VERSION}" VERSION_GREATER_EQUAL "12")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "11.8")
        OR ("${NODE_VERSION}" VERSION_GREATER_EQUAL "10.16"))
        set(NODE_API_VERSION 4)
elseif(("${NODE_VERSION}" VERSION_GREATER_EQUAL "10"))
        set(NODE_API_VERSION 3)
endif()

# We need Node-API version 6 or higher
set(NODE_API_VERSION_REQUIRED 6)
if("${NODE_API_VERSION}" LESS "${NODE_API_VERSION_REQUIRED}")
        message(FATAL_ERROR "Node-API version ${NODE_API_VERSION_REQUIRED} or higher is required. However your nodejs version '${NODE_VERSION}' can only provide Node-API version '${NODE_API_VERSION}'")
else()
        message("Found nodejs version '${NODE_VERSION}' that can provide Node-API version '${NODE_API_VERSION}' which satifies the requirement of Node-API version '${NODE_API_VERSION_REQUIRED}'")
endif()

find_program(
        NPM
        npm
        REQUIRED
)

# Include Node-API wrappers
# FIXME (aw): we want this as an requirement, not implicitely install it by ourself
execute_process(COMMAND ${NPM} install node-addon-api
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)


execute_process(COMMAND ${NODE} -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

find_path(NODEJS_INCLUDE_DIR "node_api.h"
        PATH_SUFFIXES
        "node"
        "node16"
        "nodejs/src"
        REQUIRED
)

target_include_directories(everestjs
    PRIVATE
        ${NODE_ADDON_API_DIR}
        ${NODEJS_INCLUDE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# needs c++ 14
target_compile_features(everestjs PRIVATE cxx_std_14)

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=6)
add_definitions(-DNAPI_CPP_EXCEPTIONS)

install(TARGETS everestjs
        LIBRARY
        DESTINATION everestjs/node_modules/everestjs)

install(FILES index.js
        DESTINATION everestjs/node_modules/everestjs)

install(FILES package.json
        DESTINATION everestjs/node_modules/everestjs)
