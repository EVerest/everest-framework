#pragma once

#include <framework/everest.hpp>
#include <framework/runtime.hpp>
#include <memory>
#include <string>
#include <utils/types.hpp>

#include "rust/cxx.h"

struct JsonBlob;
struct Runtime;
struct RsModuleConfig;
struct RsModuleConnections;
struct ConfigField;
enum class ConfigTypes : uint8_t;

class Module {
public:
    Module(const std::string& module_id, const std::string& prefix, const std::string& conf,
           const Everest::MQTTSettings& mqtt_settings);

    JsonBlob initialize() const;
    JsonBlob get_interface(rust::Str interface_name) const;

    void signal_ready(const Runtime& rt) const;
    void provide_command(const Runtime& rt, rust::String implementation_id, rust::String name) const;
    JsonBlob call_command(rust::Str implementation_id, size_t index, rust::Str name, JsonBlob args) const;
    void subscribe_variable(const Runtime& rt, rust::String implementation_id, size_t index, rust::String name) const;
    void publish_variable(rust::Str implementation_id, rust::Str name, JsonBlob blob) const;
    int get_log_level() const;
    std::shared_ptr<Everest::Config> get_config() const;

private:
    const std::string module_id_;
    const Everest::MQTTSettings& mqtt_settings_;
    std::shared_ptr<Everest::MQTTAbstraction> mqtt_abstraction_;
    std::shared_ptr<Everest::RuntimeSettings> rs_;
    std::shared_ptr<Everest::Config> config_;
    std::unique_ptr<Everest::Everest> handle_;
};

std::shared_ptr<Module> create_module(rust::Str module_name, rust::Str prefix, rust::Str log_config,
                                      rust::Str mqtt_broker_socket_path, rust::Str mqtt_broker_host,
                                      rust::Str mqtt_broker_port, rust::Str mqtt_everest_prefix,
                                      rust::Str mqtt_external_prefix);

rust::Vec<RsModuleConfig> get_module_configs(rust::Str module_name);
rust::Vec<RsModuleConnections> get_module_connections(rust::Str module_name);
void log2cxx(int level, int line, rust::Str file, rust::Str message);
