#ifndef FRAMEWORK_EVEREST_RUNTIME_HPP
#define FRAMEWORK_EVEREST_RUNTIME_HPP

#include <filesystem>
#include <string>

#include <everest/logging.hpp>
#include <fmt/color.h>
#include <fmt/core.h>
#include <framework/ModuleAdapter.hpp>
#include <sys/prctl.h>

#include <utils/yaml_loader.hpp>

#include <everest/compile_time_settings.hpp>

namespace boost::program_options {
class variables_map; // forward declaration
}

namespace Everest {

namespace fs = std::filesystem;

// FIXME (aw): should be everest wide or defined in liblog
const int DUMP_INDENT = 4;

// FIXME (aw): we should also define all other config keys and default
//             values here as string literals

// FIXME (aw): this needs to be made available by
namespace defaults {

// defaults:
//   PREFIX: set by cmake
//   EVEREST_NAMESPACE: everest
//   BIN_DIR: ${PREFIX}/bin
//   LIBEXEC_DIR: ${PREFIX}/libexec
//   LIB_DIR: ${PREFIX}/lib
//   SYSCONF_DIR: /etc, if ${PREFIX}==/usr, otherwise ${PREFIX}/etc
//   LOCALSTATE_DIR: /var, if ${PREFIX}==/usr, otherwise ${PREFIX}/var
//   DATAROOT_DIR: ${PREFIX}/share
//
//   modules_dir: ${LIBEXEC_DIR}${EVEREST_NAMESPACE}
//   types_dir: ${DATAROOT_DIR}${EVEREST_NAMESPACE}/types
//   interfaces_dir: ${DATAROOT_DIR}${EVEREST_NAMESPACE}/interfaces
//   schemas_dir: ${DATAROOT_DIR}${EVEREST_NAMESPACE}/schemas
//   configs_dir: ${SYSCONF_DIR}${EVEREST_NAMESPACE}
//
//   config_path: ${SYSCONF_DIR}${EVEREST_NAMESPACE}/default.yaml
//   logging_config_path: ${SYSCONF_DIR}${EVEREST_NAMESPACE}/default_logging.cfg

inline constexpr auto PREFIX = EVEREST_INSTALL_PREFIX;
inline constexpr auto NAMESPACE = EVEREST_NAMESPACE;

inline constexpr auto BIN_DIR = "bin";
inline constexpr auto LIB_DIR = "lib";
inline constexpr auto LIBEXEC_DIR = "libexec";
inline constexpr auto SYSCONF_DIR = "etc";
inline constexpr auto LOCALSTATE_DIR = "var";
inline constexpr auto DATAROOT_DIR = "share";

inline constexpr auto MODULES_DIR = "modules";
inline constexpr auto TYPES_DIR = "types";
inline constexpr auto INTERFACES_DIR = "interfaces";
inline constexpr auto SCHEMAS_DIR = "schemas";
inline constexpr auto CONFIG_NAME = "default.yaml";
inline constexpr auto LOGGING_CONFIG_NAME = "default_logging.cfg";

} // namespace defaults

std::string parse_string_option(const boost::program_options::variables_map& vm, const char* option);

const auto TERMINAL_STYLE_ERROR = fmt::emphasis::bold | fg(fmt::terminal_color::red);
const auto TERMINAL_STYLE_OK = fmt::emphasis::bold | fg(fmt::terminal_color::green);

struct BootException : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct RuntimeSettings {
    fs::path prefix;
    fs::path configs_dir;
    fs::path schemas_dir;
    fs::path modules_dir;
    fs::path interfaces_dir;
    fs::path types_dir;
    fs::path logging_config_file;
    fs::path config_file;

    nlohmann::json config;

    bool validate_schema;

    explicit RuntimeSettings(const std::string& prefix, const std::string& config);
};

struct ModuleCallbacks {
    std::function<void(ModuleAdapter module_adapter)> register_module_adapter;
    std::function<std::vector<cmd>(const json& connections)> everest_register;
    std::function<void(ModuleConfigs module_configs, const ModuleInfo& info)> init;
    std::function<void()> ready;

    ModuleCallbacks(const std::function<void(ModuleAdapter module_adapter)>& register_module_adapter,
                    const std::function<std::vector<cmd>(const json& connections)>& everest_register,
                    const std::function<void(ModuleConfigs module_configs, const ModuleInfo& info)>& init,
                    const std::function<void()>& ready);
};

class ModuleLoader {
private:
    std::unique_ptr<RuntimeSettings> runtime_settings;
    std::string module_id;
    std::string original_process_name;
    ModuleCallbacks callbacks;

    bool parse_command_line(int argc, char* argv[]);

public:
    explicit ModuleLoader(int argc, char* argv[], ModuleCallbacks callbacks);

    int initialize();
};

} // namespace Everest

#endif // FRAMEWORK_EVEREST_RUNTIME_HPP
