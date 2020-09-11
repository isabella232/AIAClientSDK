#
# Setup the Logging build.
#
# To enable logging of sensitive data, include the following option on the cmake command line:
#     -DAIA_EMIT_SENSITIVE_LOGS=ON
# Note that this option is only honored in DEBUG builds.
#

option(AIA_EMIT_SENSITIVE_LOGS "Enable Logging of sensitive information." OFF)

if (AIA_EMIT_SENSITIVE_LOGS)
        message("WARNING: Logging of sensitive information enabled!")
        add_definitions("-DAIA_EMIT_SENSITIVE_LOGS")
endif()
