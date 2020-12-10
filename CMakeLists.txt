cmake_minimum_required(VERSION 3.11.0 FATAL_ERROR)

# Set project information
project(AiaClientSDK VERSION 1.0.1 LANGUAGES C)
set(PROJECT_BRIEF "A cross-platform SDK for interacting with the AVS for AWS IoT Core")

# Installation paths
include(GNUInstallDirs)

# Enable 'test' target
enable_testing()

# Enable more verbose 'check' target
add_custom_target( check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure )

# Use C99.
set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )

# TODO: Move this to a separate CMake file specific encapsulating the build
if ( CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU" )
	SET( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall -Wextra -pedantic" )
endif()

# Do not allow in-source build.
if( ${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR} )
    message( FATAL_ERROR "In-source builds are not allowed. Please build in a separate directory, such as ${PROJECT_SOURCE_DIR}/../build." )
endif()

include(cmake/Logging.cmake)
include(cmake/CodeCoverage.cmake)

include(cmake/AiaConfigurePorts.cmake)
AiaConfigurePorts()

# Path to pull AIA config headers from
include_directories( ${PROJECT_SOURCE_DIR}/ports/include )
include_directories( ${AIA_BUTTON_FOLDER}/include)
include_directories( ${AIA_CLOCK_FOLDER}/include)
include_directories( ${AIA_CRYPTO_FOLDER}/include)
include_directories( ${AIA_IOT_FOLDER}/include)
include_directories( ${AIA_HTTP_FOLDER}/include)
include_directories( ${AIA_LWA_FOLDER}/include)
include_directories( ${AIA_MEMORY_FOLDER}/include)
include_directories( ${AIA_MICROPHONE_FOLDER}/include)
include_directories( ${AIA_REGISTRATION_FOLDER}/include)
include_directories( ${AIA_STORAGE_FOLDER}/include)
include_directories( ${AIA_COMMON_FOLDER}/include)

add_subdirectory("External")
add_subdirectory("AiaCore")
add_subdirectory("ports")
add_subdirectory("ApplicationUtilities")

# Test executables.
add_subdirectory( tests/ )

# Build demo executable.
add_subdirectory( demos/app )

# Generate/install the pkg-config file
# TODO: Generate this content dynamically from cmake variables. (ADSER-1863)
# TODO: Figure out why escaped carrots around IOT_SYSTEM_TYPES_FILE are breaking application builds. (ADSER-1864)
# TODO: Generate a cmake package config file as well. (ADSER-1865)
set(AIA_SDK_LIBS "-laiacore -laiaport -laiacryptoport -laiastorageport -laiahttpport -laiaiotport -laialwaport -laiaclockport -laiaregistrationport -laiaregulator -laiasequencer -laiaspeakermanager -laiamicrophonemanager -laiaclient -laiaemitter -laiasecretmanager -laiauxmanager -laiaalertmanager -laiaclockmanager -laiaregistrationmanager -laiaconnectionmanager -laiadispatcher -laiaexceptionmanager")
get_filename_component(IOTSDK_LIB_PATH ${IOTSDK_BASE_LIB_PATH} DIRECTORY)
set(IOT_SDK_LIBS "${IOTSDK_BASE_LIB_PATH} ${IOTSDK_MQTT_LIB_PATH} ${IOTSDK_SERIALIZER_LIB_PATH} ${IOTSDK_COMMON_LIB_PATH} -Wl,-rpath,${IOTSDK_LIB_PATH}")
if(USE_MBEDTLS)
    set(CRYPTO_LIBS "${MBEDCRYPTO_LIB_PATH}")
endif()
if(AIA_PORTAUDIO_MICROPHONE)
    set(MICROPHONE_LIBS "-laiaportaudiomicrophone")
    set(MICROPHONE_CFLAGS "-DAIA_PORTAUDIO_MICROPHONE")
endif()
if(AIA_PORTAUDIO_SPEAKER)
    set(SPEAKER_LIBS "-laiaportaudiospeaker")
    set(SPEAKER_CFLAGS "-DAIA_PORTAUDIO_SPEAKER")
endif()
if(${AIA_PORTAUDIO_MICROPHONE} OR ${AIA_PORTAUDIO_SPEAKER})
    set(PORTAUDIO_LIBS "-lportaudio")
endif()
if(AIA_OPUS_DECODER)
    set(OPUS_LIBS "-laiaopusdecoder -lopus")
    set(OPUS_CFLAGS "-DAIA_OPUS_DECODER")
endif()
if(AIA_LIBCURL_HTTP_CLIENT)
    set(HTTPCLIENT_LIBS "-laiahttpclient -lcurl")
    set(HTTPCLIENT_CFLAGS "-DAIA_LIBCURL_HTTP_CLIENT")
endif()
set(PKG_CONFIG_LIBS "${AIA_SDK_LIBS} ${IOT_SDK_LIBS} ${CRYPTO_LIBS} ${MICROPHONE_LIBS} ${SPEAKER_LIBS} ${PORTAUDIO_LIBS} ${OPUS_LIBS} ${HTTPCLIENT_LIBS}")
set(PKG_CONFIG_CFLAGS "${MICROPHONE_CFLAGS} ${SPEAKER_CFLAGS} ${OPUS_CFLAGS} ${HTTPCLIENT_CFLAGS}")
CONFIGURE_FILE(
  "${PROJECT_SOURCE_DIR}/pkg-config.pc.in"
  "${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pc"
  @ONLY
)
install(FILES "${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pc" DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
