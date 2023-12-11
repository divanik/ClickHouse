#include <mutex>
#include <Backups/WithRetries.h>

namespace DB
{


WithRetries::KeeperSettings WithRetries::KeeperSettings::fromContext(ContextPtr context)
{
    return
    {
        .keeper_max_retries = context->getSettingsRef().backup_restore_keeper_max_retries,
        .keeper_retry_initial_backoff_ms = context->getSettingsRef().backup_restore_keeper_retry_initial_backoff_ms,
        .keeper_retry_max_backoff_ms = context->getSettingsRef().backup_restore_keeper_retry_max_backoff_ms,
        .batch_size_for_keeper_multiread = context->getSettingsRef().backup_restore_batch_size_for_keeper_multiread,
        .keeper_fault_injection_probability = context->getSettingsRef().backup_restore_keeper_fault_injection_probability,
        .keeper_fault_injection_seed = context->getSettingsRef().backup_restore_keeper_fault_injection_seed,
        .keeper_value_max_size = context->getSettingsRef().backup_restore_keeper_value_max_size,
        .batch_size_for_keeper_multi = context->getSettingsRef().backup_restore_batch_size_for_keeper_multi,
    };
}

WithRetries::WithRetries(Poco::Logger * log_, zkutil::GetZooKeeper get_zookeeper_, const KeeperSettings & settings_, RenewerCallback callback_)
    : log(log_)
    , get_zookeeper(get_zookeeper_)
    , settings(settings_)
    , callback(callback_)
    , global_zookeeper_retries_info(
        log->name(),
        log,
        settings.keeper_max_retries,
        settings.keeper_retry_initial_backoff_ms,
        settings.keeper_retry_max_backoff_ms)
{}

WithRetries::RetriesControlHolder::RetriesControlHolder(const WithRetries * parent, const String & name)
    : info(parent->global_zookeeper_retries_info)
    , retries_ctl(name, info, nullptr)
    , faulty_zookeeper(parent->getFaultyZooKeeper())
{}

WithRetries::RetriesControlHolder WithRetries::createRetriesControlHolder(const String & name)
{
    return RetriesControlHolder(this, name);
}

void WithRetries::renewZooKeeper(FaultyKeeper my_faulty_zookeeper) const
{
    if (my_faulty_zookeeper->isNull() || my_faulty_zookeeper->expired())
    {
        my_faulty_zookeeper->setKeeper(get_zookeeper());
        try
        {
            /// The callback might itself fail with a new ZK error
            callback(my_faulty_zookeeper);
        }
        catch (const zkutil::KeeperException & e)
        {
            if (!Coordination::isHardwareError(e.code))
                throw;
            my_faulty_zookeeper->setKeeper(nullptr);
        }
    }
    else
    {
        my_faulty_zookeeper->setKeeper(zookeeper);
    }
}

const WithRetries::KeeperSettings & WithRetries::getKeeperSettings() const
{
    return settings;
}

WithRetries::FaultyKeeper WithRetries::getFaultyZooKeeper() const
{
    /// We need to create new instance of ZooKeeperWithFaultInjection each time a copy a pointer to ZooKeeper client there
    /// The reason is that ZooKeeperWithFaultInjection may reset the underlying pointer and there could be a race condition
    /// when the same object is used from multiple threads.
    auto faulty_zookeeper = ZooKeeperWithFaultInjection::createInstance(
        settings.keeper_fault_injection_probability, settings.keeper_fault_injection_seed, get_zookeeper(), log->name(), log);

    return faulty_zookeeper;
}


}
