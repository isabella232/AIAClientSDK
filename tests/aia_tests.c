/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file aia_tests.c
 * @brief Common test runner.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Standard includes. */
#include <string.h>

/* Error handling include. */
#include <iot_error.h>

/* Platform threads include. */
#include <platform/iot_threads.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/* This file calls a generic placeholder test runner function. The build system
 * selects the actual function by defining it. */
extern void RunTests( bool disableNetworkTests, bool disableLongTests );

/*-----------------------------------------------------------*/

/**
 * @brief Parses command line arguments.
 *
 * @param[in] argc Number of arguments passed to main().
 * @param[in] argv Arguments vector passed to main().
 * @param[out] disableNetworkTests Set to `true` if `-n` is given, `false`
 * otherwise.
 * @param[out] disableLongTests Set to `true` if `-l` is not given, `true`
 * otherwise.
 */
static void _parseArguments( int argc, char** argv, bool* disableNetworkTests,
                             bool* disableLongTests )
{
    int i = 1;
    const char* option = NULL;
    size_t optionLength = 0;

    /* Set default values. */
    *disableNetworkTests = false;
    *disableLongTests = true;

    for( i = 1; i < argc; i++ )
    {
        /* Get argument string and length. */
        option = argv[ i ];
        optionLength = strlen( option );

        /* Valid options have the format "-X", so they must be 2 characters
         * long. */
        if( optionLength != 2 )
        {
            continue;
        }

        /* The first character of a valid option must be '-'. */
        if( option[ 0 ] != '-' )
        {
            continue;
        }

        switch( option[ 1 ] )
        {
            /* Disable tests requiring network if -n is given. */
            case 'n':
                *disableNetworkTests = true;
                break;

            /* Enable long tests if -l is given. */
            case 'l':
                *disableLongTests = false;
                break;

            default:
                break;
        }
    }
}

/*-----------------------------------------------------------*/

int main( int argc, char** argv )
{
    IOT_FUNCTION_ENTRY( int, EXIT_SUCCESS );

    /* Unity setup. */
    UnityFixture.Verbose = 1;
    UnityFixture.RepeatCount = 1;
    UnityFixture.NameFilter = NULL;
    UnityFixture.GroupFilter = NULL;
    UNITY_BEGIN();

    /* Parse command-line arguments for the tests. */
    bool disableNetworkTests = false, disableLongTests = false;
    _parseArguments( argc, argv, &disableNetworkTests, &disableLongTests );

    /* Call the test runner function. */
    RunTests( disableNetworkTests, disableLongTests );

    /* Return failure if any tests failed. */
    if( UNITY_END() != 0 )
    {
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    IOT_FUNCTION_CLEANUP_BEGIN();

    IOT_FUNCTION_CLEANUP_END();
}

/*-----------------------------------------------------------*/
