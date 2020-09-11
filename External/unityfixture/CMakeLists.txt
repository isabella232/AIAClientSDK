#
# Set up the Unity test fixture for the Aia SDK.
#
# The AIA SDK depends on both the compiled unity library and the source code
# for some of the extra modules which are not built/installed by default.
#
# If Unity is not already installed in a standard system path, specify its
# location in your CMAKE_PREFIX_PATH.
#
# The AIA SDK also uses some of the optional Unity modules included in the
# "extras" folder, which are not compiled/installed as part of a normal Unity
# build, so you will also need to provide a path to the Unity source code.  If
# the source code is not already installed in a standard system path, specify
# its location in your CMAKE_PREFIX_PATH, or manually define the path as
# follows:
#     cmake <path-to-source> -DUNITY_SOURCE_DIR="/path/to/unity-source"

find_package( unity )

find_path(UNITY_SOURCE_DIR extras/fixture )
if (NOT UNITY_SOURCE_DIR)
    message(FATAL_ERROR "Could not find Unity extras.")
endif()
set(UNITY_EXTRAS_DIR "${UNITY_SOURCE_DIR}/extras")

add_library( unitymemory ${UNITY_EXTRAS_DIR}/memory/src/unity_memory.c )
target_include_directories( unitymemory PUBLIC INTERFACE "${UNITY_EXTRAS_DIR}/memory/src" )
target_link_libraries( unitymemory PUBLIC unity )

add_library( unityfixture ${UNITY_EXTRAS_DIR}/fixture/src/unity_fixture.c )
target_include_directories( unityfixture PUBLIC INTERFACE "${UNITY_EXTRAS_DIR}/fixture/src" )
target_link_libraries( unityfixture PUBLIC unitymemory )

add_custom_target( unitylibs DEPENDS unityfixture unitymemory )
