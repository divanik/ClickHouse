#include "DiskFactory.h"
#include <Poco/Logger.h>
#include "Common/logger_useful.h"

namespace DB
{
namespace ErrorCodes
{
    extern const int LOGICAL_ERROR;
    extern const int UNKNOWN_ELEMENT_IN_CONFIG;
}

DiskFactory & DiskFactory::instance()
{
    static DiskFactory factory;
    return factory;
}

void DiskFactory::registerDiskType(const String & disk_type, Creator creator)
{
    if (!registry.emplace(disk_type, creator).second)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "DiskFactory: the disk type '{}' is not unique", disk_type);
}

DiskPtr DiskFactory::create(
    const String & name,
    const Poco::Util::AbstractConfiguration & config,
    const String & config_prefix,
    ContextPtr context,
    const DisksMap & map,
    bool attach,
    bool custom_disk) const
{
    const auto disk_type = config.getString(config_prefix + ".type", "local");

    std::cerr << "Config prefix: " << config_prefix << std::endl;
    std::cerr << "Disk_type: " << disk_type << std::endl;
    for (const auto & [key, value] : registry)
        std::cerr << "Key in registry: " << key << std::endl;

    const auto found = registry.find(disk_type);
    if (found == registry.end())
    {
        throw Exception(ErrorCodes::UNKNOWN_ELEMENT_IN_CONFIG,
                        "DiskFactory: the disk '{}' has unknown disk type: {}", name, disk_type);
    }

    const auto & disk_creator = found->second;
    return disk_creator(name, config, config_prefix, context, map, attach, custom_disk);
}

}
