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
 * @file aia_stream_buffer_tests.c
 * @brief Tests for AiaDataStreamBuffer_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* AiaDataStreamBuffer_t headers */
#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_writer.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaDataStreamBuffer_t tests.
 */
TEST_GROUP( AiaStreamBufferTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaDataStreamBuffer_t tests.
 */
TEST_SETUP( AiaStreamBufferTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaDataStreamBuffer_t tests.
 */
TEST_TEAR_DOWN( AiaStreamBufferTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaStreamBufferTests )
{
    RUN_TEST_CASE( AiaStreamBufferTests, Creation );
    RUN_TEST_CASE( AiaStreamBufferTests, CreateWriter );
    RUN_TEST_CASE( AiaStreamBufferTests, CreateReader );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderRead );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderSeek );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderTell );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderClose );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderGetId );
    RUN_TEST_CASE( AiaStreamBufferTests, ReaderGetWordSize );
    RUN_TEST_CASE( AiaStreamBufferTests, WriterWrite );
    RUN_TEST_CASE( AiaStreamBufferTests, WriterTell );
    RUN_TEST_CASE( AiaStreamBufferTests, WriterClose );
    RUN_TEST_CASE( AiaStreamBufferTests, WriterGetWordSize );
}

TEST( AiaStreamBufferTests, Creation )
{
    static const size_t MAX_READERS = 2;
    static const size_t WORDSIZE_REQUIRED = sizeof( uint16_t );
    static const size_t MULTIPLE_WORDS = 2;
    uint32_t maxReaders, wordCount, wordSize;
    for( maxReaders = 0; maxReaders <= MAX_READERS; ++maxReaders )
    {
        for( wordSize = 0; wordSize <= WORDSIZE_REQUIRED; ++wordSize )
        {
            for( wordCount = 0; wordCount <= MULTIPLE_WORDS; ++wordCount )
            {
                /* Should fail to create an SDS with an empty buffer. */
                void* buffer = NULL;
                AiaDataStreamBuffer_t* sds = AiaDataStreamBuffer_Create(
                    buffer, 0, wordSize, maxReaders );
                TEST_ASSERT_FALSE( sds );

                /* Should be able to create an SDS which can only hold one word.
                 */
                buffer = AiaCalloc( 1, wordSize );
                sds = AiaDataStreamBuffer_Create( buffer, wordSize, wordSize,
                                                  maxReaders );
                if( wordSize == 0 )
                {
                    TEST_ASSERT_FALSE( sds );
                    continue;
                }
                TEST_ASSERT_TRUE( sds );
                TEST_ASSERT_EQUAL( 1U, AiaDataStreamBuffer_GetDataSize( sds ) );
                TEST_ASSERT_EQUAL( wordSize,
                                   AiaDataStreamBuffer_GetWordSize( sds ) );
                TEST_ASSERT_EQUAL( maxReaders,
                                   AiaDataStreamBuffer_GetMaxReaders( sds ) );
                AiaDataStreamBuffer_Destroy( sds );
                AiaFree( buffer );

                buffer = AiaCalloc( 1, wordSize * wordCount );
                sds = AiaDataStreamBuffer_Create( buffer, wordSize * wordCount,
                                                  wordSize, maxReaders );
                if( wordCount == 0 )
                {
                    TEST_ASSERT_FALSE( sds );
                    continue;
                }
                TEST_ASSERT_TRUE( sds );
                TEST_ASSERT_EQUAL( wordCount,
                                   AiaDataStreamBuffer_GetDataSize( sds ) );
                TEST_ASSERT_EQUAL( wordSize,
                                   AiaDataStreamBuffer_GetWordSize( sds ) );
                TEST_ASSERT_EQUAL( AiaDataStreamBuffer_GetMaxReaders( sds ),
                                   maxReaders );
                AiaDataStreamBuffer_Destroy( sds );
                AiaFree( buffer );
            }
        }
    }

    /*
    Verify create() detects the upper limit on maxReaders, and that the limit
    meets/exceeds SDK requirements.
    */
    for( maxReaders = 1; maxReaders < UINT32_MAX; maxReaders <<= 1 )
    {
        static const size_t WORDSIZE = 1;
        static const size_t WORDCOUNT = 1;
        size_t bufferSize = WORDSIZE * WORDCOUNT;
        void* buffer = AiaCalloc( 1, bufferSize );
        TEST_ASSERT_TRUE( buffer );
        AiaDataStreamBuffer_t* sds = AiaDataStreamBuffer_Create(
            buffer, bufferSize, WORDSIZE, maxReaders );
        if( !sds )
        {
            AiaFree( buffer );
            break;
        }
        TEST_ASSERT_EQUAL( AiaDataStreamBuffer_GetMaxReaders( sds ),
                           maxReaders );
        AiaDataStreamBuffer_Destroy( sds );
        AiaFree( buffer );
    }
    TEST_ASSERT_GREATER_OR_EQUAL( MAX_READERS, maxReaders );

    /*
    Verify create() detects the upper limit on wordSize, and that the limit
    meets/exceeds SDK requirements.
    */
    for( wordSize = 1; wordSize < UINT32_MAX; wordSize <<= 1 )
    {
        static const size_t WORDCOUNT = 1;
        static const size_t MAXREADERS = 1;
        size_t bufferSize = wordSize * WORDCOUNT;
        void* buffer = AiaCalloc( 1, bufferSize );
        AiaDataStreamBuffer_t* sds = AiaDataStreamBuffer_Create(
            buffer, bufferSize, wordSize, MAXREADERS );
        if( !sds )
        {
            AiaFree( buffer );
            break;
        }
        TEST_ASSERT_EQUAL( wordSize, AiaDataStreamBuffer_GetWordSize( sds ) );
        AiaDataStreamBuffer_Destroy( sds );
        AiaFree( buffer );
    }
    TEST_ASSERT_GREATER_THAN( WORDSIZE_REQUIRED, wordSize );
}

TEST( AiaStreamBufferTests, CreateWriter )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 1;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 1;

    /* Initialize the buffer. */
    size_t bufferSize = WORDSIZE * WORDCOUNT;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a writer without forcing. */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );

    /*
    Verify that we can't create another writer while the first one is still
    open.
    */
    AiaDataStreamWriter_t* writer2 = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_FALSE( writer2 );

    /*
    Verify that we can create another writer after the first one is closed.
    */
    AiaDataStreamWriter_Close( writer );
    writer2 = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer2 );

    /* Verify that we can create another writer after deleting. */
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamWriter_Destroy( writer2 );
    writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );

    /*
    Verify that we can delete a closed writer after creating another, without
    affecting the other (open) writer.
    */
    AiaDataStreamWriter_Close( writer );
    writer2 = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer2 );
    AiaDataStreamWriter_Destroy( writer );
    writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    /* We can't create another writer while a writer is attached. */
    TEST_ASSERT_FALSE( writer );

    /* Verify that we can force-create a writer when another is still open. */
    AiaDataStreamWriter_Destroy( writer2 );
    writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );
    writer2 = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_FALSE( writer2 );
    writer2 = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, true );
    TEST_ASSERT_TRUE( writer2 );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamWriter_Destroy( writer2 );

    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, CreateReader )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 1;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 2;

    /* Initialize the buffer. */
    size_t bufferSize = WORDSIZE * WORDCOUNT;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a reader without forcing. */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );

    /*
    Verify that we can create a second reader while the first one is still
    open.
    */
    AiaDataStreamReader_t* reader2 = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader2 );

    /*
    Verify that we can't create a third reader while the first two are
    still open.
    */
    AiaDataStreamReader_t* reader3 = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_FALSE( reader3 );

    /*
    Verify that we can't create a third reader after the first one is closed.
    */
    AiaDataStreamReader_Close(
        reader, 0, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER );
    reader3 = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_FALSE( reader3 );

    /*
    Verify that we can create another reader after deleting the one that is
    closed.
    */
    AiaDataStreamReader_Destroy( reader );
    reader3 = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader3 );

    /* Verify that we can create a readers with a specific ID. */
    static const size_t FIXED_ID = 0;
    AiaDataStreamReader_Destroy( reader2 );
    AiaDataStreamReader_Destroy( reader3 );
    reader = AiaDataStreamBuffer_CreateReaderWithId(
        sds, FIXED_ID, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false,
        false );
    TEST_ASSERT_TRUE( reader );

    /*
    Verify that we can't create a reader with an ID that is already in use.
    */
    reader2 = AiaDataStreamBuffer_CreateReaderWithId(
        sds, FIXED_ID, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false,
        false );
    TEST_ASSERT_FALSE( reader2 );
    AiaDataStreamReader_Destroy( reader );
    reader2 = AiaDataStreamBuffer_CreateReaderWithId(
        sds, FIXED_ID, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false,
        false );
    TEST_ASSERT_TRUE( reader2 );

    /*
    Verify that we can force-create a reader with an ID that is already in
    use.
    */
    reader = AiaDataStreamBuffer_CreateReaderWithId(
        sds, FIXED_ID, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false, true );
    TEST_ASSERT_TRUE( reader );

    /*
    Verify that onlyReadNewData=false puts the reader at the newest data in
    the buffer.
    */
    uint8_t* buf = (uint8_t*)AiaCalloc( WORDCOUNT, WORDSIZE );
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING, false );
    TEST_ASSERT_TRUE( writer );
    TEST_ASSERT_EQUAL( WORDCOUNT,
                       AiaDataStreamWriter_Write( writer, buf, WORDCOUNT ) );
    AiaDataStreamReader_Destroy( reader );
    reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );
    TEST_ASSERT_EQUAL(
        WORDCOUNT,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    AiaDataStreamReader_Destroy( reader );
    reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, true );
    TEST_ASSERT_TRUE( reader );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );

    AiaFree( buf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamReader_Destroy( reader2 );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderRead )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 2;
    static const size_t WORDCOUNT = 2;
    static const size_t MAXREADERS = 2;
    static const uint8_t WRITEFILL = 1;
    static const uint8_t READFILL = 0;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create nonblocking reader. */
    AiaDataStreamReader_t* nonblocking = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( nonblocking );

    /* Verify bad parameter handling. */
    uint8_t* readBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT * 2 );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID,
        AiaDataStreamReader_Read( nonblocking, NULL, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID,
                       AiaDataStreamReader_Read( nonblocking, readBuf, 0 ) );

    /* Verify read detects unopened stream (no writer). */
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
        AiaDataStreamReader_Read( nonblocking, readBuf, WORDCOUNT ) );

    /* Attach a writer. */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );

    /* Verify read detects empty stream. */
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
        AiaDataStreamReader_Read( nonblocking, readBuf, WORDCOUNT ) );

    /* Verify correct number of bytes are read. */
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    /* Fill with pattern */
    for( size_t i = 0; i < WORDSIZE * WORDCOUNT; ++i )
    {
        writeBuf[ i ] = WRITEFILL;
    }
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    for( size_t i = 0; i < WORDSIZE * WORDCOUNT; ++i )
    {
        readBuf[ i ] = READFILL;
    }
    TEST_ASSERT_EQUAL(
        WORDCOUNT / 2,
        AiaDataStreamReader_Read( nonblocking, readBuf, WORDCOUNT / 2 ) );

    size_t mismatchIndex;
    for( mismatchIndex = 0; mismatchIndex < WORDSIZE * WORDCOUNT;
         ++mismatchIndex )
    {
        if( readBuf[ mismatchIndex ] != writeBuf[ mismatchIndex ] )
        {
            break;
        }
    }
    TEST_ASSERT_EQUAL( WORDSIZE * WORDCOUNT / 2, mismatchIndex );

    /* Read more data than the buffer contains. */
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        nonblocking, 0,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( WORDCOUNT, AiaDataStreamReader_Read(
                                      nonblocking, readBuf, WORDCOUNT * 2 ) );

    /* Verify reader detect overflows. */
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN,
        AiaDataStreamReader_Read( nonblocking, readBuf, WORDCOUNT * 2 ) );

    AiaFree( writeBuf );
    AiaFree( readBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamReader_Destroy( nonblocking );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderSeek )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 2;
    static const size_t WORDCOUNT = 10;
    static const size_t MAXREADERS = 2;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a reader. */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );
    AiaDataStreamIndex_t readerPos = 0;

    /* Attach a writer and fill half of the buffer with a pattern. */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );
    AiaDataStreamIndex_t writerPos = 0;
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    for( size_t i = 0; i < WORDSIZE * WORDCOUNT; ++i )
    {
        writeBuf[ i ] = i;
    }
    size_t writeWords = WORDCOUNT / 2;
    TEST_ASSERT_EQUAL(
        writeWords, AiaDataStreamWriter_Write( writer, writeBuf, writeWords ) );
    writerPos += writeWords;

    /* --- AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER --- */

    /*
    Verify we can seek forward from the current read position to the middle
    of the written data.
    */
    size_t seekWords = 1;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    readerPos += seekWords;
    uint8_t* readBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    ssize_t readWords = 1;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    readerPos += readWords;

    /*
    Verify we can seek forward from the current read position to the end of
    the written data.
    */
    seekWords = writeWords - readerPos;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    readerPos += seekWords;
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );

    /*
    Verify we can seek forward from the current read position beyond the end
    of the written data.
    */
    seekWords = 1;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    readerPos += seekWords;

    /* --- AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER --- */

    /*
    Verify we can seek backward from the current read position to the middle
    of the written data.
    */
    seekWords = 2;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    readerPos -= seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /*
    Verify we can seek backward from the current read position to the
    beginning of the written data.
    */
    seekWords = writeWords;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    readerPos -= seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /*
    Verify we can't seek backward from the current read position before the
    beginning of the written data.
    */
    seekWords = readerPos + 1;
    TEST_ASSERT_FALSE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /* --- AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER --- */

    /*
    Verify we can seek backward from the current write position to the end of
    the written data.
    */
    seekWords = 0;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    readerPos = writerPos - seekWords;
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );

    /*
    Verify we can seek backward from the current write position to the middle
    of the written data.
    */
    seekWords = 1;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    readerPos = writerPos - seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /*
    Verify we can seek backward from the current write position to the
    beginning of the written data.
    */
    seekWords = writeWords;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    readerPos = writerPos - seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /*
    Verify we can't seek backward from the current write position before the
    beginning of the written data.
    */
    seekWords = writeWords + 1;
    TEST_ASSERT_FALSE( AiaDataStreamReader_Seek(
        reader, seekWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /* --- AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE --- */

    /* Verify we can seek directly to the end of the written data. */
    seekWords = writerPos;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    readerPos = seekWords;
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );

    /*
    Verify we can seek directly to a position beyond the end of the written
    data.
    */
    seekWords = writerPos + 1;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    readerPos = seekWords;

    /* Verify we can seek directly to the middle of the written data. */
    seekWords = writerPos - 2;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    readerPos = seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /* Verify we can seek directly to the beginning of the written data. */
    seekWords = 0;
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, seekWords, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    readerPos = seekWords;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( writeBuf[ readerPos * WORDSIZE ], readBuf[ 0 ] );
    readerPos += readWords;

    /* Verify that we can't seek to a position that has been overwritten. */
    writeWords = WORDCOUNT;
    TEST_ASSERT_EQUAL(
        writeWords, AiaDataStreamWriter_Write( writer, writeBuf, writeWords ) );
    writerPos += writeWords;
    seekWords = 0;
    TEST_ASSERT_FALSE( AiaDataStreamReader_Seek(
        reader, seekWords, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );

    AiaFree( readBuf );
    AiaFree( writeBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderTell )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 2;
    static const size_t WORDCOUNT = 10;
    static const size_t MAXREADERS = 2;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a reader. */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );
    AiaDataStreamIndex_t readerPos = 0;

    /* Check initial position. */
    TEST_ASSERT_EQUAL(
        0U, AiaDataStreamReader_Tell(
                reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );

    /* Attach a writer */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );

    /* Check initial reader position relative to writer. */
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );

    /* Fill half the buffer. */
    AiaDataStreamIndex_t writerPos = 0;
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    size_t writeWords = WORDCOUNT / 2;
    TEST_ASSERT_EQUAL(
        writeWords, AiaDataStreamWriter_Write( writer, writeBuf, writeWords ) );
    writerPos += writeWords;

    /*
    Verify position relative to writer has changed, but others are unchanged.
    */
    TEST_ASSERT_EQUAL(
        0U, AiaDataStreamReader_Tell(
                reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    TEST_ASSERT_EQUAL(
        writerPos,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );

    /*
    Read a word, then verify that position relative to writer and absolute
    have changed, but others are unchanged.
    */
    uint8_t* readBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( readBuf );
    size_t readWords = 1;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    readerPos += readWords;
    TEST_ASSERT_EQUAL(
        readerPos,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    TEST_ASSERT_EQUAL(
        writerPos - readerPos,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );

    /*
    Read remaining words, then verify that position relative to writer is
    zero, aboslute has changed, others are unchanged.
    */
    readWords = writerPos - readerPos;
    TEST_ASSERT_EQUAL( readWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    readerPos += readWords;
    TEST_ASSERT_EQUAL(
        readerPos,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER ) );
    TEST_ASSERT_EQUAL(
        0U,
        AiaDataStreamReader_Tell(
            reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );

    AiaFree( readBuf );
    AiaFree( writeBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderClose )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 2;
    static const size_t WORDCOUNT = 10;
    static const size_t MAXREADERS = 2;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a reader. */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );

    /* Attach a writer and fill the buffer. */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    size_t writeWords = WORDCOUNT;
    TEST_ASSERT_EQUAL(
        writeWords, AiaDataStreamWriter_Write( writer, writeBuf, writeWords ) );

    /* Request reader to close immediately and verify that it does so. */
    uint8_t* readBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( readBuf );
    size_t readWords = 2;
    AiaDataStreamReader_Close(
        reader, 0, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );

    /* Request the reader to close later and verify that does so. */
    size_t closeWords = 2;
    readWords = writeWords;
    AiaDataStreamReader_Close(
        reader, closeWords,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER );
    TEST_ASSERT_EQUAL( writeWords - closeWords,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED,
                       AiaDataStreamReader_Read( reader, readBuf, readWords ) );

    AiaFree( readBuf );
    AiaFree( writeBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderGetId )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 1;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 10;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /*
    Create all readers and verify that their IDs are unique and less than
    AiaDataStreamBuffer_GetMaxReaders().
    */
    AiaDataStreamReader_t** readers =
        AiaCalloc( MAXREADERS, sizeof( readers ) );
    TEST_ASSERT_TRUE( readers );

    for( size_t i = 0; i < MAXREADERS; ++i )
    {
        AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
            sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
        TEST_ASSERT_TRUE( reader );
        TEST_ASSERT_LESS_THAN( AiaDataStreamBuffer_GetMaxReaders( sds ),
                               AiaDataStreamReader_GetId( reader ) );
        TEST_ASSERT_TRUE( AiaDataStreamBuffer_IsReaderEnabled(
            sds, AiaDataStreamReader_GetId( reader ) ) );
        readers[ i ] = reader;
    }
    /* Creating another reader beyond MAXREADERS should fail. */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_FALSE( reader );
    for( size_t i = 0; i < MAXREADERS; ++i )
    {
        AiaDataStreamReader_Destroy( readers[ i ] );
    }

    /*
    Create all readers with manually-assigned IDs and make sure they read
    back correctly.
    */
    for( size_t i = 0; i < MAXREADERS; ++i )
    {
        readers[ i ] = AiaDataStreamBuffer_CreateReaderWithId(
            sds, i, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false, false );
        TEST_ASSERT_TRUE( readers[ i ] );
        TEST_ASSERT_EQUAL( i, AiaDataStreamReader_GetId( readers[ i ] ) );
    }
    for( size_t i = 0; i < MAXREADERS; ++i )
    {
        AiaDataStreamReader_Destroy( readers[ i ] );
    }

    AiaFree( readers );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, ReaderGetWordSize )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t MINWORDSIZE = 1;
    static const size_t MAXWORDSIZE = 8;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 1;

    for( size_t wordSize = MINWORDSIZE; wordSize <= MAXWORDSIZE; ++wordSize )
    {
        size_t bufferSize = WORDCOUNT * wordSize;
        void* buffer = AiaCalloc( 1, bufferSize );
        TEST_ASSERT_TRUE( buffer );
        AiaDataStreamBuffer_t* sds = AiaDataStreamBuffer_Create(
            buffer, bufferSize, wordSize, MAXREADERS );
        TEST_ASSERT_TRUE( sds );
        AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
            sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
        TEST_ASSERT_TRUE( reader );
        TEST_ASSERT_EQUAL( wordSize,
                           AiaDataStreamReader_GetWordSize( reader ) );

        AiaDataStreamReader_Destroy( reader );
        AiaDataStreamBuffer_Destroy( sds );
        AiaFree( buffer );
    }
}

TEST( AiaStreamBufferTests, WriterWrite )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 2;
    static const size_t WORDCOUNT = 2;
    static const size_t MAXREADERS = 1;

    /* Initialize three streams. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer1 = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer1 );
    AiaDataStreamBuffer_t* sds1 =
        AiaDataStreamBuffer_Create( buffer1, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds1 );
    void* buffer2 = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer2 );
    AiaDataStreamBuffer_t* sds2 =
        AiaDataStreamBuffer_Create( buffer2, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds2 );
    void* buffer3 = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer3 );
    AiaDataStreamBuffer_t* sds3 =
        AiaDataStreamBuffer_Create( buffer3, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds3 );

    /* Create nonblockable, all-or-nothing, and nonblocking writers. */
    AiaDataStreamWriter_t* nonblockable = AiaDataStreamBuffer_CreateWriter(
        sds1, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( nonblockable );
    AiaDataStreamWriter_t* allOrNothing = AiaDataStreamBuffer_CreateWriter(
        sds2, AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING, false );
    TEST_ASSERT_TRUE( allOrNothing );
    AiaDataStreamWriter_t* nonblocking = AiaDataStreamBuffer_CreateWriter(
        sds3, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( nonblocking );

    /* Verify bad parameter handling. */
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID,
        AiaDataStreamWriter_Write( nonblockable, NULL, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( AiaDataStreamWriter_Write( nonblockable, writeBuf, 0 ),
                       AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID,
        AiaDataStreamWriter_Write( allOrNothing, NULL, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID,
                       AiaDataStreamWriter_Write( allOrNothing, writeBuf, 0 ) );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID,
        AiaDataStreamWriter_Write( nonblocking, NULL, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID,
                       AiaDataStreamWriter_Write( nonblocking, writeBuf, 0 ) );

    /* Verify all writers can write data to their buffers. */
    size_t writeWords = WORDCOUNT / 2;
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       nonblockable, writeBuf, writeWords ) );
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       allOrNothing, writeBuf, writeWords ) );
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       nonblocking, writeBuf, writeWords ) );

    /* Verify nonblockable writer can overflow the buffer without blocking. */
    writeWords = WORDCOUNT;
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       nonblockable, writeBuf, writeWords ) );

    /* Verify all-or-nothing writer can't overflow the buffer. */
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK,
        AiaDataStreamWriter_Write( allOrNothing, writeBuf, writeWords ) );

    /* Verify non-blocking writer can fill the buffer. */
    TEST_ASSERT_EQUAL( WORDCOUNT / 2, AiaDataStreamWriter_Write(
                                          nonblocking, writeBuf, WORDCOUNT ) );

    /* Verify non-blocking writer can't write to a full buffer. */
    TEST_ASSERT_EQUAL(
        0, AiaDataStreamWriter_Write( nonblocking, writeBuf, WORDCOUNT ) );

    /* Verify we can switch policies and overwrite data. */
    AiaDataStreamWriter_SetPolicy( nonblocking,
                                   AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE );
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       nonblocking, writeBuf, writeWords ) );
    AiaDataStreamWriter_SetPolicy( nonblockable,
                                   AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING );
    TEST_ASSERT_EQUAL(
        0, AiaDataStreamWriter_Write( nonblockable, writeBuf, WORDCOUNT ) );

    /* Verify all-or-nothing writer can't overrun a reader who is in the future.
     */
    AiaDataStreamReader_t* reader = AiaDataStreamBuffer_CreateReader(
        sds2, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( reader );
    TEST_ASSERT_TRUE( AiaDataStreamReader_Seek(
        reader, WORDCOUNT,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER ) );
    writeWords = WORDCOUNT * 2;
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK,
        AiaDataStreamWriter_Write( allOrNothing, writeBuf, writeWords ) );

    /* Verify all-or-nothing writer can discard data that will not be read by a
     * reader who is waiting in the future. */
    writeWords = WORDCOUNT + WORDCOUNT / 2;
    TEST_ASSERT_EQUAL( writeWords, AiaDataStreamWriter_Write(
                                       allOrNothing, writeBuf, writeWords ) );

    AiaFree( writeBuf );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamWriter_Destroy( nonblocking );
    AiaDataStreamWriter_Destroy( allOrNothing );
    AiaDataStreamWriter_Destroy( nonblockable );
    AiaDataStreamBuffer_Destroy( sds3 );
    AiaFree( buffer3 );
    AiaDataStreamBuffer_Destroy( sds2 );
    AiaFree( buffer2 );
    AiaDataStreamBuffer_Destroy( sds1 );
    AiaFree( buffer1 );
}

TEST( AiaStreamBufferTests, WriterTell )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 1;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 1;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a writer/ */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING, false );
    TEST_ASSERT_TRUE( writer );

    /* Verify initial position. */
    TEST_ASSERT_EQUAL( 0U, AiaDataStreamWriter_Tell( writer ) );

    /* Verify position changes after a successful write. */
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( WORDCOUNT, AiaDataStreamWriter_Tell( writer ) );

    /* Verify position doesn't change after an unsuccessful write. */
    TEST_ASSERT_EQUAL(
        0, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    TEST_ASSERT_EQUAL( WORDCOUNT, AiaDataStreamWriter_Tell( writer ) );

    AiaFree( writeBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, WriterClose )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t WORDSIZE = 1;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 1;

    /* Initialize an sds. */
    size_t bufferSize = WORDCOUNT * WORDSIZE;
    void* buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE( buffer );
    AiaDataStreamBuffer_t* sds =
        AiaDataStreamBuffer_Create( buffer, bufferSize, WORDSIZE, MAXREADERS );
    TEST_ASSERT_TRUE( sds );

    /* Create a writer. */
    AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_TRUE( writer );

    /* Verify it can write before closing, but not after. */
    uint8_t* writeBuf = AiaCalloc( 1, WORDSIZE * WORDCOUNT );
    TEST_ASSERT_TRUE( writeBuf );
    TEST_ASSERT_EQUAL(
        WORDCOUNT, AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );
    AiaDataStreamWriter_Close( writer );
    TEST_ASSERT_EQUAL(
        AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED,
        AiaDataStreamWriter_Write( writer, writeBuf, WORDCOUNT ) );

    AiaFree( writeBuf );
    AiaDataStreamWriter_Destroy( writer );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
}

TEST( AiaStreamBufferTests, WriterGetWordSize )
{
    TEST_ASSERT_TRUE( 1 );
    static const size_t MINWORDSIZE = 1;
    static const size_t MAXWORDSIZE = 8;
    static const size_t WORDCOUNT = 1;
    static const size_t MAXREADERS = 1;
    for( size_t wordSize = MINWORDSIZE; wordSize <= MAXWORDSIZE; ++wordSize )
    {
        size_t bufferSize = WORDCOUNT * wordSize;
        void* buffer = AiaCalloc( 1, bufferSize );
        TEST_ASSERT_TRUE( buffer );
        AiaDataStreamBuffer_t* sds = AiaDataStreamBuffer_Create(
            buffer, bufferSize, wordSize, MAXREADERS );
        TEST_ASSERT_TRUE( sds );
        AiaDataStreamWriter_t* writer = AiaDataStreamBuffer_CreateWriter(
            sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
        TEST_ASSERT_TRUE( writer );
        TEST_ASSERT_EQUAL( wordSize,
                           AiaDataStreamWriter_GetWordSize( writer ) );

        AiaDataStreamWriter_Destroy( writer );
        AiaDataStreamBuffer_Destroy( sds );
        AiaFree( buffer );
    }
}
