#pragma once

#include <Storages/StorageS3Settings.h>
#include "IO/Archives/IArchiveReader.h"
#include "base/types.h"
#include "config.h"

#if USE_AWS_S3

#include <memory>

#include <IO/HTTPCommon.h>
#include <IO/ParallelReadBuffer.h>
#include <IO/ReadBuffer.h>
#include <IO/ReadSettings.h>
#include <IO/ReadBufferFromFileBase.h>
#include <IO/WithFileName.h>

#include <aws/s3/model/GetObjectResult.h>

namespace DB
{
/**
 * Perform S3 HTTP GET request and provide response to read.
 */
class ReadBufferFromS3 : public ReadBufferFromFileBase
{
private:
    std::shared_ptr<const S3::Client> client_ptr;
    String bucket;
    String key;
    String version_id;
    const S3Settings::RequestSettings request_settings;

    /// These variables are atomic because they can be used for `logging only`
    /// (where it is not important to get consistent result)
    /// from separate thread other than the one which uses the buffer for s3 reading.
    std::atomic<off_t> offset = 0;
    std::atomic<off_t> read_until_position = 0;

    std::optional<Aws::S3::Model::GetObjectResult> read_result;
    std::unique_ptr<ReadBuffer> impl;

    LoggerPtr log = getLogger("ReadBufferFromS3");

public:
    ReadBufferFromS3(
        std::shared_ptr<const S3::Client> client_ptr_,
        const String & bucket_,
        const String & key_,
        const String & version_id_,
        const S3Settings::RequestSettings & request_settings_,
        const ReadSettings & settings_,
        bool use_external_buffer = false,
        size_t offset_ = 0,
        size_t read_until_position_ = 0,
        bool restricted_seek_ = false,
        std::optional<size_t> file_size = std::nullopt);

    ~ReadBufferFromS3() override = default;

    bool nextImpl() override;

    off_t seek(off_t off, int whence) override;

    off_t getPosition() override;

    size_t getFileSize() override;

    void setReadUntilPosition(size_t position) override;
    void setReadUntilEnd() override;

    size_t getFileOffsetOfBufferEnd() const override { return offset; }

    bool supportsRightBoundedReads() const override { return true; }

    String getFileName() const override { return bucket + "/" + key; }

    size_t readBigAt(char * to, size_t n, size_t range_begin, const std::function<bool(size_t)> & progress_callback) const override;

    bool supportsReadAt() override { return true; }

private:
    std::unique_ptr<ReadBuffer> initialize(size_t attempt);

    /// If true, if we destroy impl now, no work was wasted. Just for metrics.
    bool atEndOfRequestedRangeGuess();

    /// Call inside catch() block if GetObject fails. Bumps metrics, logs the error.
    /// Returns true if the error looks retriable.
    bool processException(Poco::Exception & e, size_t read_offset, size_t attempt) const;

    Aws::S3::Model::GetObjectResult sendRequest(size_t attempt, size_t range_begin, std::optional<size_t> range_end_incl) const;

    ReadSettings read_settings;

    bool use_external_buffer;

    /// There is different seek policy for disk seek and for non-disk seek
    /// (non-disk seek is applied for seekable input formats: orc, arrow, parquet).
    bool restricted_seek;

    bool read_all_range_successfully = false;
};

// class ArchiveOpenFromS3File
// {
// protected:
//     ArchiveOpenFromS3File(
//         std::shared_ptr<const S3::Client> client_ptr_,
//         const String & bucket_,
//         const String & key_,
//         const String & version_id_,
//         const S3Settings::RequestSettings & request_settings_,
//         const ReadSettings & settings_,
//         String path_in_archive_,
//         bool use_external_buffer_,
//         size_t offset_,
//         size_t read_until_position_,
//         bool restricted_seek_,
//         std::optional<size_t> file_size_);

//     std::unique_ptr<ReadBufferFromFileBase> readFile() { return archive_reader->readFile(path_in_archive, true); }

// private:
//     std::unique_ptr<ReadBufferFromS3> file_buffer_from_s3;
//     std::shared_ptr<IArchiveReader> archive_reader;
//     String path_in_archive;
// };

// class ReadBufferFromS3Archive : protected ArchiveOpenFromS3File, public std::unique_ptr<ReadBufferFromFileBase>
// {
// public:
//     ReadBufferFromS3Archive(
//         std::shared_ptr<const S3::Client> client_ptr_,
//         const String & bucket_,
//         const String & key_,
//         const String & version_id_,
//         const S3Settings::RequestSettings & request_settings_,
//         const ReadSettings & settings_,
//         const String & path_in_archive,
//         bool use_external_buffer = false,
//         size_t offset_ = 0,
//         size_t read_until_position_ = 0,
//         bool restricted_seek_ = false,
//         std::optional<size_t> file_size_ = std::nullopt)
//     // : ArchiveOpenFromS3File(
//     //     client_ptr_,
//     //     bucket_,
//     //     key_,
//     //     version_id_,
//     //     request_settings_,
//     //     settings_,
//     //     path_in_archive,
//     //     use_external_buffer,
//     //     offset_,
//     //     read_until_position_,
//     //     restricted_seek_,
//     //     file_size_)
//     // , std::unique_ptr<ReadBufferFromFileBase>(readFile())
//     {
//     }
// };
// std::unique_ptr<ReadBufferFromFileBase> get
}
#endif
