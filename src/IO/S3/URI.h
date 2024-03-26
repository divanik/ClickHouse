#pragma once

#include <optional>
#include <string>

#include "Common/Exception.h"
#include "config.h"

#if USE_AWS_S3

#include <Poco/URI.h>

namespace DB::S3
{
namespace ErrorCodes
{
extern const int BAD_ARGUMENTS;
}

/**
 * Represents S3 URI.
 *
 * The following patterns are allowed:
 * s3://bucket/key
 * http(s)://endpoint/bucket/key
 */
struct URI
{
    Poco::URI uri;
    // Custom endpoint if URI scheme is not S3.
    std::string endpoint;
    std::string bucket;
    std::string key;
    std::string version_id;
    std::string storage_name;
    std::optional<std::string> archive_pattern;

    bool is_virtual_hosted_style;

    URI() = default;
    explicit URI(const std::string & uri_);
    void addRegionToURI(const std::string & region);

    static void validateBucket(const std::string & bucket, const Poco::URI & uri);

private:
    void parseFileSource(std::string source, std::string & filename, std::optional<std::string> & path_to_archive)
    {
        size_t pos = source.find("::");
        if (pos == std::string::npos)
        {
            filename = std::move(source);
            return;
        }

        std::string_view path_to_archive_view = std::string_view{source}.substr(0, pos);
        while (path_to_archive_view.ends_with(' '))
            path_to_archive_view.remove_suffix(1);

        if (path_to_archive_view.empty())
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Path to archive is empty");

        path_to_archive = path_to_archive_view;

        std::string_view filename_view = std::string_view{source}.substr(pos + 2);
        while (filename_view.front() == ' ')
            filename_view.remove_prefix(1);

        if (filename_view.empty())
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Filename is empty");

        filename = filename_view;
    }
};

}

#endif
