#include "DisksApp.h"
#include <exception>
#include <stdexcept>
#include "ICommand.h"

#include <Disks/registerDisks.h>

#include <Common/TerminalSize.h>
#include <Formats/registerFormats.h>


#include "Common/logger_useful.h"


namespace DB
{

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}

void DisksApp::printHelpMessage(const ProgramOptionsDescription & command_option_description)
{
    std::optional<ProgramOptionsDescription> help_description =
        createOptionsDescription("Help Message for clickhouse-disks", getTerminalWidth());

    help_description->add(command_option_description);

    std::cout << "ClickHouse disk management tool\n";
    std::cout << "Usage: ./clickhouse-disks [OPTION]\n";
    std::cout << "clickhouse-disks\n\n";

    for (const auto & current_command : supported_commands)
    {
        std::cout << command_descriptions[current_command]->command_name;
        bool was = false;
        for (const auto & [alias_name, alias_command_name] : aliases)
        {
            if (alias_command_name == current_command)
            {
                if (was)
                    std::cout << ",";
                else
                    std::cout << "(";
                std::cout << alias_name;
                was = true;
            }
        }
        std::cout << (was ? ")" : "") << " \t" << command_descriptions[current_command]->description << "\n\n";
    }

    std::cout << command_option_description << '\n';
}

[[noreturn]] void DisksApp::stopWithUnknownCommandName(const ProgramOptionsDescription & command_option_description)
{
    printHelpMessage(command_option_description);
    std::cerr << "Command name couldn't be resolved" << std::endl;
    exit(1);
}

size_t DisksApp::findCommandPos(std::vector<String> & common_arguments, const ProgramOptionsDescription & options_description)
{
    for (size_t i = 0; i < common_arguments.size(); i++)
        if (supported_commands.contains(common_arguments[i]) || (aliases.contains(common_arguments[i])))
            return i + 1;
    stopWithUnknownCommandName(options_description);
}

String DisksApp::getDefaultConfigFileName()
{
    return "/etc/clickhouse-server/config.xml";
}

void DisksApp::addOptions(
    ProgramOptionsDescription & options_description_,
    boost::program_options::positional_options_description & positional_options_description
)
{
    options_description_.add_options()("help,h", "Print common help message")("config-file,C", po::value<String>(), "Set config file")(
        "disk", po::value<String>(), "Set disk name")("command_name", po::value<String>(), "Name for command to do")(
        "save-logs", "Save logs to a file")("log-level", po::value<String>(), "Logging level")(
        "subargs", po::value<std::vector<std::string>>(), "Arguments for command");

    positional_options_description.add("command_name", 1).add("subargs", -1);

    command_descriptions.emplace("list-disks", makeCommandListDisks());
    command_descriptions.emplace("list", makeCommandList());
    command_descriptions.emplace("move", makeCommandMove());
    command_descriptions.emplace("remove", makeCommandRemove());
    command_descriptions.emplace("link", makeCommandLink());
    command_descriptions.emplace("copy", makeCommandCopy());
    command_descriptions.emplace("write", makeCommandWrite());
    command_descriptions.emplace("read", makeCommandRead());
    command_descriptions.emplace("mkdir", makeCommandMkDir());
#ifdef CLICKHOUSE_CLOUD
    command_descriptions.emplace("packed-io", makeCommandPackedIO());
#endif
}

void DisksApp::processOptions()
{
    if (options.count("config-file"))
        config().setString("config-file", options["config-file"].as<String>());
    if (options.count("disk"))
        config().setString("disk", options["disk"].as<String>());
    if (options.count("save-logs"))
        config().setBool("save-logs", true);
    if (options.count("log-level"))
        config().setString("log-level", options["log-level"].as<String>());
}

DisksApp::~DisksApp()
{
    global_context->shutdown();
}

void DisksApp::init(std::vector<String> & common_arguments)
{
    for (auto & argument : common_arguments)
        std::cerr << "Common arguments: " << argument << std::endl;
    stopOptionsProcessing();

    ProgramOptionsDescription options_description{createOptionsDescription("clickhouse-disks", getTerminalWidth())};

    po::positional_options_description positional_options_description;

    addOptions(options_description, positional_options_description);

    size_t command_pos = findCommandPos(common_arguments, options_description);
    std::vector<String> global_flags(command_pos);
    command_arguments.resize(common_arguments.size() - command_pos);
    copy(common_arguments.begin(), common_arguments.begin() + command_pos, global_flags.begin());
    copy(common_arguments.begin() + command_pos, common_arguments.end(), command_arguments.begin());

    parseAndCheckOptions(options_description, positional_options_description, common_arguments);

    for (const auto & description : options_description.options())
        std::cerr << "Description: " << description->description() << std::endl;

    po::notify(options);

    if (options.count("help"))
    {
        printHelpMessage(options_description);
        exit(0); // NOLINT(concurrency-mt-unsafe)
    }

    if (!supported_commands.contains(command_name))
    {
        stopWithUnknownCommandName(options_description);
    }

    processOptions();
}

void DisksApp::parseAndCheckOptions(
    ProgramOptionsDescription & options_description_,
    boost::program_options::positional_options_description & positional_options_description,
    std::vector<String> & arguments)
{
    std::cout << "Before parsing" << std::endl;
    auto parser = po::command_line_parser(arguments)
        .options(options_description_)
        .positional(positional_options_description)
        .allow_unregistered();
    std::cout << "After parsing" << std::endl;
    po::parsed_options parsed = parser.run();
    std::cout << "After parsing 2" << std::endl;

    for (size_t i = 0; i < parsed.options.size(); ++i)
        std::cout << parsed.options[i].string_key << std::endl;
    // throw std::runtime_error("What's the fuck");
    po::store(parsed, options);
    // std::cout << "After parsing 3" << std::endl;

    auto positional_arguments = po::collect_unrecognized(parsed.options, po::collect_unrecognized_mode::include_positional);
    for (const auto & arg : positional_arguments)
    {
        std::cout << "Pos arg " << arg << std::endl;
        if (supported_commands.contains(arg))
        {
            command_name = arg;
            break;
        }
        auto it = aliases.find(arg);
        if (it != aliases.end())
        {
            command_name = it->second;
            break;
        }
    }
}

int DisksApp::main(const std::vector<String> & /*args*/)
{
    // LOG_DEBUG(&Poco::Logger::get("Shrek "), "Shrek");
    std::vector<std::string> keys;
    config().keys(keys);
    for (auto & key : keys)
        std::cerr << "Key: " << key << std::endl;
    if (config().has("config-file") || fs::exists(getDefaultConfigFileName()))
    {
        String config_path = config().getString("config-file", getDefaultConfigFileName());
        ConfigProcessor config_processor(config_path, false, false);
        ConfigProcessor::setConfigPath(fs::path(config_path).parent_path());
        auto loaded_config = config_processor.loadConfig();
        config().add(loaded_config.configuration.duplicate(), false, false);
    }
    else
    {
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "No config-file specified");
    }

    config().keys(keys);
    for (auto & key : keys)
        std::cerr << "Key2: " << key << std::endl;

    if (config().has("save-logs"))
    {
        auto log_level = config().getString("log-level", "trace");
        Poco::Logger::root().setLevel(Poco::Logger::parseLevel(log_level));

        auto log_path = config().getString("logger.clickhouse-disks", "/var/log/clickhouse-server/clickhouse-disks.log");
        Poco::Logger::root().setChannel(Poco::AutoPtr<Poco::FileChannel>(new Poco::FileChannel(log_path)));
    }
    else
    {
        auto log_level = config().getString("log-level", "none");
        Poco::Logger::root().setLevel(Poco::Logger::parseLevel(log_level));
    }

    registerDisks(/* global_skip_access_check= */ true);
    registerFormats();

    shared_context = Context::createShared();
    global_context = Context::createGlobal(shared_context.get());

    global_context->makeGlobalContext();
    global_context->setApplicationType(Context::ApplicationType::DISKS);

    String path = config().getString("path", DBMS_DEFAULT_PATH);
    global_context->setPath(path);

    std::cerr << "Command name: " << command_name << std::endl;

    auto & command = command_descriptions[command_name];

    auto command_options = command->getCommandOptions();
    std::vector<String> args;
    if (command_options)
    {
        auto parser = po::command_line_parser(command_arguments).options(*command_options).allow_unregistered();
        po::parsed_options parsed = parser.run();
        po::store(parsed, options);
        po::notify(options);

        args = po::collect_unrecognized(parsed.options, po::collect_unrecognized_mode::include_positional);
        command->processOptions(config(), options);
    }
    else
    {
        auto parser = po::command_line_parser(command_arguments).options({}).allow_unregistered();
        po::parsed_options parsed = parser.run();
        args = po::collect_unrecognized(parsed.options, po::collect_unrecognized_mode::include_positional);
    }

    std::unordered_set<std::string> disks
    {
        config().getString("disk", "default"),
        config().getString("disk-from", config().getString("disk", "default")),
        config().getString("disk-to", config().getString("disk", "default")),
    };

    auto validator = [&disks](
        const Poco::Util::AbstractConfiguration & config,
        const std::string & disk_config_prefix,
        const std::string & disk_name)
    {
        if (!disks.contains(disk_name))
            return false;

        const auto disk_type = config.getString(disk_config_prefix + ".type", "local");

        if (disk_type == "cache")
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Disk type 'cache' of disk {} is not supported by clickhouse-disks", disk_name);

        return true;
    };

    constexpr auto config_prefix = "storage_configuration.disks";
    auto disk_selector = std::make_shared<DiskSelector>();
    disk_selector->initialize(config(), config_prefix, global_context, validator);

    command->execute(args, disk_selector, config());

    return Application::EXIT_OK;
}

}

int mainEntryClickHouseDisks(int argc, char ** argv)
{
    try
    {
        DB::DisksApp app;
        std::vector<String> common_arguments{argv + 1, argv + argc};
        app.init(common_arguments);
        return app.run();
    }
    catch (const DB::Exception & e)
    {
        std::cerr << DB::getExceptionMessage(e, false) << std::endl;
        return 1;
    }
    catch (const boost::program_options::error & e)
    {
        std::cerr << "Bad arguments: " << e.what() << std::endl;
        return DB::ErrorCodes::BAD_ARGUMENTS;
    }
    catch (...)
    {
        std::cerr << DB::getCurrentExceptionMessage(true) << std::endl;
        return 1;
    }
}
