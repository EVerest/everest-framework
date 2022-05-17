#include <filesystem>
#include <fstream>
#include <boost/program_options.hpp>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11_json/pybind11_json.hpp>

#include <framework/ModuleAdapter.hpp>
#include <framework/everest.hpp>
#include <framework/runtime.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;
namespace py = pybind11;

struct Log {
    static void debug(const std::string& message) {
        EVLOG(debug) << message;
    }
    static void info(const std::string& message) {
        EVLOG(info) << message;
    }
    static void warning(const std::string& message) {
        EVLOG(warning) << message;
    }
    static void error(const std::string& message) {
        EVLOG(error) << message;
    }
    static void critical(const std::string& message) {
        EVLOG(critical) << message;
    }
};

struct CmdWithArguments {
    std::function<json(json json_value)> cmd;
    json arguments;
};

struct Reqs {
    std::map<std::string, std::map<std::string, std::function<void(std::function<void(json json_value)>)>>> vars;
    std::map<std::string, std::map<std::string, std::function<void(json json_value)>>> pub_vars;
    std::map<std::string, std::map<std::string, CmdWithArguments>> call_cmds;
    std::map<std::string, std::map<std::string, json>> pub_cmds;
    bool enable_external_mqtt = false;
};

struct EverestPyCmd {
    std::string impl_id;
    std::string cmd_name;
    std::function<json(json parameters)> handler;
};

struct EverestPy {
    ::Everest::ModuleCallbacks module_callbacks;
    std::function<void(const Reqs& reqs)> pre_init;
    std::function<std::vector<EverestPyCmd>(const json& connections)> everest_register;
};

EverestPy everest_py;

void register_module_adapter_callback(
    const std::function<void(::Everest::ModuleAdapter module_adapter)>& register_module_adapter) {
    everest_py.module_callbacks.register_module_adapter = register_module_adapter;
}

void register_everest_register_callback(
    const std::function<std::vector<EverestPyCmd>(const json& connections)>& everest_register) {
    everest_py.everest_register = everest_register;
}

void register_init_callback(const std::function<void(ModuleConfigs module_configs, const ModuleInfo& info)>& init) {
    everest_py.module_callbacks.init = init;
}

void register_pre_init_callback(const std::function<void(const Reqs& reqs)>& pre_init) {
    everest_py.pre_init = pre_init;
}

void register_ready_callback(const std::function<void()>& ready) {
    everest_py.module_callbacks.ready = ready;
}

int initialize(fs::path main_dir, fs::path configs_dir, fs::path schemas_dir, fs::path modules_dir,
               fs::path interfaces_dir, fs::path logging_config, fs::path config_file, bool dontvalidateschema,
               std::string module_id) {
    auto rs = Everest::RuntimeSettings(main_dir, configs_dir, schemas_dir, modules_dir, interfaces_dir, logging_config,
                                       config_file, dontvalidateschema);
    Everest::Logging::init(rs.logging_config.string(), module_id);

    try {
        Everest::Config config = Everest::Config(rs.schemas_dir.string(), rs.config_file.string(),
                                                 rs.modules_dir.string(), rs.interfaces_dir.string());

        if (!config.contains(module_id)) {
            EVLOG(error) << fmt::format("Module id '{}' not found in config!", module_id);
            return 2;
        }

        const std::string module_identifier = config.printable_identifier(module_id);
        EVLOG(info) << fmt::format("Initializing framework for module {}...", module_identifier);
        EVLOG(debug) << fmt::format("Setting process name to: '{}'...", module_identifier);
        int prctl_return = prctl(PR_SET_NAME, module_identifier.c_str());
        if (prctl_return == 1) {
            EVLOG(warning) << fmt::format("Could not set process name to '{}'.", module_identifier);
        }
        Everest::Logging::update_process_name(module_identifier);

        // NOLINTNEXTLINE(concurrency-mt-unsafe): not problematic that this function is not threadsafe here
        const char* mqtt_server_address = std::getenv("MQTT_SERVER_ADDRESS");
        if (mqtt_server_address == nullptr) {
            mqtt_server_address = "localhost";
        }

        // NOLINTNEXTLINE(concurrency-mt-unsafe): not problematic that this function is not threadsafe here
        const char* mqtt_server_port = std::getenv("MQTT_SERVER_PORT");
        if (mqtt_server_port == nullptr) {
            mqtt_server_port = "1883";
        }

        Everest::Everest& everest = Everest::Everest::get_instance(module_id, config, rs.validate_schema,
                                                                   mqtt_server_address, mqtt_server_port);

        EVLOG(info) << fmt::format("Initializing module {}...", module_identifier);

        const std::string& module_name = config.get_main_config()[module_id]["module"].get<std::string>();
        auto module_manifest = config.get_manifests()[module_name];
        auto module_impls = config.get_interfaces()[module_name];

        Reqs reqs;
        reqs.enable_external_mqtt = module_manifest.at("enable_external_mqtt");

        // module provides
        for (const auto& impl_definition : module_impls.items()) {
            const auto& impl_id = impl_definition.key();
            const auto& impl_intf = module_impls[impl_id];

            std::map<std::string, std::function<void(std::function<void(json json_value)>)>> impl_vars_prop;
            if (impl_intf.contains("vars")) {
                for (const auto& var_entry : impl_intf["vars"].items()) {
                    const auto& var_name = var_entry.key();
                    reqs.pub_vars[impl_id][var_name] = [&everest, impl_id, var_name](json json_value) {
                        everest.publish_var(impl_id, var_name, json_value);
                    };
                }
            }

            if (impl_intf.contains("cmds")) {
                for (const auto& cmd_entry : impl_intf["cmds"].items()) {
                    const auto& cmd_name = cmd_entry.key();
                    reqs.pub_cmds[impl_id][cmd_name] = cmd_entry.value();
                }
            }
        }

        // module requires (uses)
        for (auto& requirement : module_manifest["requires"].items()) {
            auto const& requirement_id = requirement.key();
            json req_route_list = config.resolve_requirement(module_id, requirement_id);
            // if this was a requirement with min_connections == 1 and max_connections == 1,
            // this will be simply a single connection, but an array of connections otherwise
            // (this array can have only one entry, if only one connection was provided, though)
            const bool is_list = req_route_list.is_array();
            if (!is_list) {
                req_route_list = json::array({req_route_list});
            }

            reqs.vars = {};
            reqs.vars[requirement_id] = {};

            for (size_t i = 0; i < req_route_list.size(); i++) {
                auto req_route = req_route_list[i];
                const std::string& requirement_module_id = req_route["module_id"];
                const std::string& requirement_impl_id = req_route["implementation_id"];
                std::string interface_name = req_route["required_interface"].get<std::string>();
                auto requirement_impl_intf = config.get_interface_definition(interface_name);
                auto requirement_vars = Everest::Config::keys(requirement_impl_intf["vars"]);
                auto requirement_cmds = Everest::Config::keys(requirement_impl_intf["cmds"]);

                for (auto var_name : requirement_vars) {
                    reqs.vars[requirement_id][var_name] = [&everest, requirement_id, i,
                                                           var_name](std::function<void(json json_value)> callback) {
                        everest.subscribe_var({requirement_id, i}, var_name, callback);
                    };
                }

                for (auto const& cmd_name : requirement_cmds) {
                    reqs.call_cmds[requirement_id][cmd_name] = {
                        [&everest, requirement_id, i, cmd_name](json parameters) {
                            return everest.call_cmd({requirement_id, i}, cmd_name, parameters);
                        },
                        requirement_impl_intf.at("cmds").at(cmd_name).at("arguments")};
                }

            }
        }

        if (!everest.connect()) {
            EVLOG(critical) << fmt::format("Cannot connect to MQTT broker at {}:{}", mqtt_server_address,
                                           mqtt_server_port);
            return 1;
        }

        Everest::ModuleAdapter module_adapter;

        module_adapter.call = [&everest](const Requirement& req, const std::string& cmd_name, Parameters args) {
            return everest.call_cmd(req, cmd_name, args);
        };

        module_adapter.publish = [&everest](const std::string& param1, const std::string& param2, Value param3) {
            return everest.publish_var(param1, param2, param3);
        };

        module_adapter.subscribe = [&everest](const Requirement& req, const std::string& var_name,
                                              const ValueCallback& callback) {
            return everest.subscribe_var(req, var_name, callback);
        };

        // NOLINTNEXTLINE(modernize-avoid-bind): prefer bind here for readability
        module_adapter.ext_mqtt_publish = std::bind(&::Everest::Everest::external_mqtt_publish, &everest,
                                                    std::placeholders::_1, std::placeholders::_2);

        // NOLINTNEXTLINE(modernize-avoid-bind): prefer bind here for readability
        module_adapter.ext_mqtt_subscribe = std::bind(&::Everest::Everest::provide_external_mqtt_handler, &everest,
                                                      std::placeholders::_1, std::placeholders::_2);

        everest_py.module_callbacks.register_module_adapter(module_adapter);

        everest_py.pre_init(reqs);

        // FIXME (aw): would be nice to move this config related thing toward the module_init function
        std::vector<EverestPyCmd> cmds =
            everest_py.everest_register(config.get_main_config()[module_id]["connections"]);

        for (auto const& command : cmds) {
            everest.provide_cmd(command.impl_id, command.cmd_name, command.handler);
        }

        auto module_configs = config.get_module_configs(module_id);
        const auto module_info = config.get_module_info(module_id);

        everest_py.module_callbacks.init(module_configs, module_info);

        everest.spawn_main_loop_thread();

        // register the modules ready handler with the framework
        // this handler gets called when the global ready signal is received
        everest.register_on_ready_handler(everest_py.module_callbacks.ready);

        // the module should now be ready
        everest.signal_ready();
    } catch (boost::exception& e) {
        EVLOG(critical) << fmt::format("Caught top level boost::exception:\n{}",
                                       boost::diagnostic_information(e, true));
    } catch (std::exception& e) {
        EVLOG(critical) << fmt::format("Caught top level std::exception:\n{}", boost::diagnostic_information(e, true));
    }

    return 0;
}

PYBIND11_MODULE(everestpy, m) {
    m.doc() = R"pbdoc(
        Python bindings for EVerest
        -----------------------
        .. currentmodule:: everestpy
        .. autosummary::
           :toctree: _generate
           log
    )pbdoc";

    py::class_<EverestPyCmd>(m, "EverestPyCmd")
        .def(py::init<>())
        .def_readwrite("impl_id", &EverestPyCmd::impl_id)
        .def_readwrite("cmd_name", &EverestPyCmd::cmd_name)
        .def_readwrite("handler", &EverestPyCmd::handler);

    py::bind_vector<std::vector<EverestPyCmd>>(m, "EverestPyCmdVector");

    py::class_<Log>(m, "log")
        .def(py::init<>())
        .def_static("debug", &Log::debug)
        .def_static("info", &Log::info)
        .def_static("warning", &Log::warning)
        .def_static("error", &Log::error)
        .def_static("critical", &Log::critical);

    py::class_<::Everest::ModuleAdapter>(m, "ModuleAdapter")
        .def(py::init<>())
        .def_readwrite("ext_mqtt_publish", &::Everest::ModuleAdapter::ext_mqtt_publish)
        .def_readwrite("ext_mqtt_subscribe", &::Everest::ModuleAdapter::ext_mqtt_subscribe);

    py::class_<ModuleConfigs>(m, "ModuleConfigs").def(py::init<>());
    py::class_<ModuleInfo>(m, "ModuleInfo").def(py::init<>());
    py::class_<CmdWithArguments>(m, "CmdWithArguments")
        .def(py::init<>())
        .def_readwrite("cmd", &CmdWithArguments::cmd)
        .def_readwrite("arguments", &CmdWithArguments::arguments);
    py::class_<Reqs>(m, "Reqs")
        .def(py::init<>())
        .def_readwrite("vars", &Reqs::vars)
        .def_readwrite("pub_vars", &Reqs::pub_vars)
        .def_readwrite("call_cmds", &Reqs::call_cmds)
        .def_readwrite("pub_cmds", &Reqs::pub_cmds)
        .def_readwrite("enable_external_mqtt", &Reqs::enable_external_mqtt);

    m.def("init",
          [](const std::string& main_dir, const std::string& configs_dir, const std::string& schemas_dir,
             const std::string& modules_dir, const std::string& interfaces_dir, const std::string& logging_config,
             const std::string& config_file, bool dontvalidateschema, const std::string& module_id) {
              initialize(fs::path(main_dir), fs::path(configs_dir), fs::path(schemas_dir), fs::path(modules_dir),
                         fs::path(interfaces_dir), fs::path(logging_config), fs::path(config_file), dontvalidateschema,
                         module_id);
          });

    m.def("register_module_adapter_callback", &register_module_adapter_callback);
    m.def("register_everest_register_callback", &register_everest_register_callback);
    m.def("register_init_callback", &register_init_callback);
    m.def("register_pre_init_callback", &register_pre_init_callback);
    m.def("register_ready_callback", &register_ready_callback);

    m.attr("__version__") = "0.1";
}
