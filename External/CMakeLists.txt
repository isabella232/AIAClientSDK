set(USE_MBEDTLS ON CACHE BOOL "Enables use of mbedTLS for cryptography and random numbers.")
if(USE_MBEDTLS)
    add_subdirectory("mbedtls")
endif()

add_subdirectory("awsiotcsdk")
add_subdirectory("unityfixture")

set(AIA_PORTAUDIO_MICROPHONE ON CACHE BOOL "Enables use of portaudio for microphone input.")
set(AIA_PORTAUDIO_SPEAKER ON CACHE BOOL "Enables use of portaudio for speaker output.")
if(${AIA_PORTAUDIO_MICROPHONE} OR ${AIA_PORTAUDIO_SPEAKER})
    add_subdirectory("portaudio")
endif()

set(AIA_OPUS_DECODER ON CACHE BOOL "Enables use of libopus for audio decoding.")
if(${AIA_OPUS_DECODER})
    add_subdirectory("opus")
endif()

set(AIA_LIBCURL_HTTP_CLIENT ON CACHE BOOL "Enables use of libcurl as the AIA http client.")
if(${AIA_LIBCURL_HTTP_CLIENT})
    add_subdirectory("libcurl")
endif()

