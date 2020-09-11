#
# Set up AWS IoT C SDK libraries for the the Aia SDK.
#
# The AWS IoT C SDK does not currently have an install script, so the compiled
# libraries end up in the build directory, while the headers remain in the
# source directory.  The AIA SDK depends on both headers and libraries, so it
# needs to know both the build and source directories.
#
# You can specify these paths in your CMAKE_PREFIX_PATH as follows:
#     cmake <path-to-source>
#           -DCMAKE_PREFIX_PATH="/path/to/iotsdk;/path/to/iotsdk-build/output;other;paths"
#
# Alternately, you can give specific paths directly as cmake parameters as
# follows:
#     cmake <path-to-source>
#           -DIOTSDK_BASE_LIB_PATH=<path-to-libiotbase> 
#           -DIOTSDK_MQTT_LIB_PATH=<path-to-libiotmqtt>
#           -DIOTSDK_SERIALIZER_LIB_PATH=<path-to-libiotserializer>
#           -DIOTSDK_COMMON_LIB_PATH=<path-to-libawsiotcommon>
#           -DIOTSDK_SOURCE_DIR=<path-to-iotsdk-sources> 
#           -DIOTSDK_PLATFORM_NAME=<platform-name>
#
# IMPORTANT: The AIA SDK assumes that the IoT SDK is not built with IOT_BUILD_TESTS=ON.

find_path(IOTSDK_SOURCE_DIR libraries/standard/common/include/iot_init.h)
if (NOT IOTSDK_SOURCE_DIR)
    message(FATAL_ERROR "Could not find AWS IoT SDK source directory.")
endif()

if (NOT IOTSDK_PLATFORM_NAME)
    if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
        set(IOTSDK_PLATFORM_NAME "macos" CACHE STRING "IoT Platform Name.")
    elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
        set(IOTSDK_PLATFORM_NAME "posix" CACHE STRING "IoT Platform Name.")
    else()
        message( FATAL_ERROR "Unknown system \"${CMAKE_SYSTEM_NAME}\"; must pass the AWS IoT SDK platform name (see ${IOTSDK_SOURCE_DIR}/ports for supported platforms)." )
    endif()
endif()

# Platform types file.
string( CONCAT IOTSDK_SYSTEM_TYPES_FILE
	"${IOTSDK_SOURCE_DIR}/ports/${IOTSDK_PLATFORM_NAME}/include/"
	"iot_platform_types_${IOTSDK_PLATFORM_NAME}.h" )

# Cache it for global visibility (needed in pkg-config generation).
set( IOTSDK_SYSTEM_TYPES_FILE "${IOTSDK_SYSTEM_TYPES_FILE}" CACHE STRING "IoT SDK System Types File" )

find_library(IOTSDK_BASE_LIB_PATH iotbase)
if (NOT IOTSDK_BASE_LIB_PATH)
    message(FATAL_ERROR "Could not find iotbase library.")
endif()

add_library(iotbase INTERFACE IMPORTED GLOBAL)
target_link_libraries(iotbase INTERFACE "${IOTSDK_BASE_LIB_PATH}")
target_compile_definitions(iotbase INTERFACE -DIOT_SYSTEM_TYPES_FILE="${IOTSDK_SYSTEM_TYPES_FILE}" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/demos")
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/ports/common/include" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/libraries" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/libraries/platform" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/libraries/standard/common/include" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/libraries/standard/mqtt/include" )
target_include_directories(iotbase INTERFACE "${IOTSDK_SOURCE_DIR}/libraries/aws/common/include")

find_library(IOTSDK_MQTT_LIB_PATH iotmqtt)
if (NOT IOTSDK_MQTT_LIB_PATH)
    message(FATAL_ERROR "Could not find iotmqtt library.")
endif()
add_library(iotmqtt INTERFACE IMPORTED GLOBAL)
target_link_libraries(iotbase INTERFACE "${IOTSDK_MQTT_LIB_PATH}" iotbase)

find_library(IOTSDK_SERIALIZER_LIB_PATH iotserializer)
if (NOT IOTSDK_SERIALIZER_LIB_PATH)
    message(FATAL_ERROR "Could not find iotserializer library.")
endif()
add_library(iotserializer INTERFACE IMPORTED GLOBAL)
target_link_libraries(iotserializer INTERFACE "${IOTSDK_SERIALIZER_LIB_PATH}" iotbase)

find_library(IOTSDK_COMMON_LIB_PATH awsiotcommon)
if (NOT IOTSDK_COMMON_LIB_PATH)
    message(FATAL_ERROR "Could not find awsiotcommon library.")
endif()
add_library(awsiotcommon INTERFACE IMPORTED GLOBAL)
target_link_libraries(awsiotcommon INTERFACE "${IOTSDK_COMMON_LIB_PATH}" iotbase)
