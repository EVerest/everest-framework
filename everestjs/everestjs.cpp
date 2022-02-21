// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2021 Pionix GmbH and Contributors to EVerest
#include <everest/exceptions.hpp>
#include <everest/logging.hpp>

#include <framework/everest.hpp>

#include <napi.h>

#include <sys/prctl.h>

#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include "conversions.hpp"
#include "js_exec_ctx.hpp"
#include "utils.hpp"

namespace EverestJs {

struct EvModCtx {
    EvModCtx(Everest::Everest& everest, const Everest::json& module_manifest, const Napi::Env& env) :
        everest(everest),
        module_manifest(module_manifest),
        framework_ready_deferred(Napi::Promise::Deferred::New(env)),
        framework_ready_flag{false} {
        framework_ready_promise = Napi::Persistent(framework_ready_deferred.Promise());
    };
    Everest::Everest& everest;
    // std::unique_ptr<Everest::Config> config;
    const Everest::json module_manifest;

    const Napi::Promise::Deferred framework_ready_deferred;
    Napi::Reference<Napi::Promise> framework_ready_promise;
    bool framework_ready_flag;
    Napi::ObjectReference js_module_ref;

    std::unique_ptr<JsExecCtx> js_cb;

    std::map<std::pair<Requirement, std::string>, Napi::FunctionReference> var_subscriptions;
    std::map<std::pair<std::string, std::string>, Napi::FunctionReference> cmd_handlers;

    std::map<std::string, Napi::FunctionReference> mqtt_subscriptions;
};

// FIXME (aw): this could go into a class with singleton pattern
EvModCtx* ctx;

static Napi::Value publish_var(const std::string& impl_id, const std::string& var_name,
                               const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();
    const auto& env = info.Env();

    try {
        ctx->everest.publish_var(impl_id, var_name, convertToJson(info[0]));
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return info.Env().Undefined();
}

static Napi::Value setup_cmd_handler(const std::string& impl_id, const std::string& cmd_name,
                                     const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    const auto& env = info.Env();

    try {
        const auto& handler = info[0].As<Napi::Function>();

        auto cmd_key = std::make_pair(impl_id, cmd_name);
        auto& cmd_handlers = ctx->cmd_handlers;

        if (cmd_handlers.find(cmd_key) != cmd_handlers.end()) {
            EVTHROW(EVEXCEPTION(Everest::EverestApiError, "Attaching more than one handler to ", impl_id, "->",
                                cmd_name, " is not yet supported!"));
        }

        cmd_handlers.insert({cmd_key, Napi::Persistent(handler)});
        // FIXME (aw): in principle we could also pass this reference down to js_cb

        ctx->everest.provide_cmd(impl_id, cmd_name, [cmd_key](Everest::json input) -> Everest::json {
            Everest::json result;

            ctx->js_cb->exec(
                [&input, &cmd_key](Napi::Env& env) {
                    const auto& arg = convertToNapiValue(env, input);
                    std::vector<napi_value> args{ctx->cmd_handlers[cmd_key].Value(), ctx->js_module_ref.Value(), arg};
                    return args;
                },
                [&result, &cmd_key](const Napi::CallbackInfo& info, bool err) {
                    if (err) {
                        const std::string error_msg =
                            "Call into " + cmd_key.first + "->" + cmd_key.second + " got rejected";
                        throw Napi::Error::New(info.Env(), error_msg);
                    }

                    result = convertToJson(info[0]);
                });

            return result;
        });
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return env.Undefined();
}

static Napi::Value set_var_subscription_handler(const Requirement& req, const std::string& var_name,
                                                const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();
    const auto& env = info.Env();

    try {
        const auto& handler = info[0].As<Napi::Function>();

        auto sub_key = std::make_pair(req, var_name);
        auto& var_subs = ctx->var_subscriptions;

        if (var_subs.find(sub_key) != var_subs.end()) {
            // FIXME (aw): error handling, if var is already subscribed
            EVTHROW(EVEXCEPTION(Everest::EverestApiError, "Subscribing to ", req.id, "->", var_name,
                                " more than once is not yet supported!"));
        }
        var_subs.insert({sub_key, Napi::Persistent(handler)});

        // FIXME (aw): in principle we could also pass this reference down to js_cb
        ctx->everest.subscribe_var(req, var_name, [sub_key](Everest::json input) {
            ctx->js_cb->exec(
                [&input, &sub_key](Napi::Env& env) {
                    const auto& arg = convertToNapiValue(env, input);
                    std::vector<napi_value> args{ctx->var_subscriptions[sub_key].Value(), ctx->js_module_ref.Value(),
                                                 arg};
                    return args;
                },
                nullptr);
        });
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return env.Undefined();
}

static Napi::Value signal_ready(const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    const auto& env = info.Env();
    Napi::Value retval;

    try {
        ctx->everest.signal_ready();
        retval = ctx->framework_ready_promise.Value();
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return retval;
}

void framework_ready_handler() {
    BOOST_LOG_FUNCTION();
    // resolving the promise must be done inside the main js context!
    auto handle_ready_js = [](const Napi::CallbackInfo& info) -> Napi::Value {
        ctx->framework_ready_flag = true;
        ctx->framework_ready_deferred.Resolve(ctx->js_module_ref.Value());
        return info.Env().Undefined();
    };

    ctx->js_cb->exec(
        [handle_ready_js](Napi::Env& env) {
            std::vector<napi_value> args{Napi::Function::New(env, handle_ready_js)};
            return args;
        },
        nullptr);
}

static Napi::Value mqtt_publish(const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    const auto& env = info.Env();
    try {
        const auto& topic_alias = info[0].ToString().Utf8Value();
        const auto& data = info[1].ToString().Utf8Value();

        ctx->everest.external_mqtt_publish(topic_alias, data);
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return env.Undefined();
}

static Napi::Value mqtt_subscribe(const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    const auto& env = info.Env();

    try {
        const auto& topic_alias = info[0].ToString().Utf8Value();
        const auto& handler = info[1].As<Napi::Function>();

        auto& mqtt_subs = ctx->mqtt_subscriptions;

        if (mqtt_subs.find(topic_alias) != mqtt_subs.end()) {
            EVTHROW(EVEXCEPTION(Everest::EverestApiError, "Subscribing to external mqtt topic alias '", topic_alias,
                                "' more than once is not yet supported!"));
        }

        ctx->mqtt_subscriptions.insert({topic_alias, Napi::Persistent(handler)});

        ctx->everest.provide_external_mqtt_handler(topic_alias, [topic_alias](std::string data) {
            ctx->js_cb->exec(
                [&topic_alias, &data](Napi::Env& env) {
                    // in case we're not ready, the mod argument of the subscribe handler will be undefined, so no
                    // module related functions can be used
                    Napi::Value module_ref = (ctx->framework_ready_flag) ? ctx->js_module_ref.Value() : env.Undefined();
                    std::vector<napi_value> args = {ctx->mqtt_subscriptions[topic_alias].Value(), module_ref,
                                                    Napi::String::New(env, data)};
                    return args;
                },
                nullptr);
        });
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return env.Undefined();
}

static Napi::Value call_cmd(const Requirement& req, const std::string& cmd_name, const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    const auto& env = info.Env();
    Napi::Value cmd_result;

    try {
        const auto& argument = convertToJson(info[0]);
        const auto& retval = ctx->everest.call_cmd(req, cmd_name, argument);

        cmd_result = convertToNapiValue(info.Env(), retval["retval"]);
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return cmd_result;
}

static Napi::Value boot_module(const Napi::CallbackInfo& info) {
    BOOST_LOG_FUNCTION();

    auto env = info.Env();

    auto available_handlers_prop = Napi::Object::New(env);

    try {

        auto module_this = info.This().ToObject();
        const Napi::Object& settings = info[0].ToObject();
        const Napi::Function& callback_wrapper = info[1].As<Napi::Function>();

        const auto& module_id = settings.Get("module").ToString().Utf8Value();
        const auto& main_dir = settings.Get("main_dir").ToString().Utf8Value();
        const auto& schemas_dir = settings.Get("schemas_dir").ToString().Utf8Value();
        const auto& modules_dir = settings.Get("modules_dir").ToString().Utf8Value();
        const auto& interfaces_dir = settings.Get("interfaces_dir").ToString().Utf8Value();
        const auto& types_dir = settings.Get("types_dir").ToString().Utf8Value();
        const auto& config_file = settings.Get("config_file").ToString().Utf8Value();
        const auto& log_config_file = settings.Get("log_config_file").ToString().Utf8Value();
        const bool validate_schema = settings.Get("validate_schema").ToBoolean().Value();

        const auto& mqtt_server_address = settings.Get("mqtt_server_address").ToString().Utf8Value();
        const auto& mqtt_server_port = settings.Get("mqtt_server_port").ToString().Utf8Value();

        // initialize logging as early as possible
        Everest::Logging::init(log_config_file, module_id);

        auto config = std::make_unique<Everest::Config>(schemas_dir, config_file, modules_dir, interfaces_dir, types_dir);
        if (!config->contains(module_id)) {
            EVTHROW(EVEXCEPTION(Everest::EverestConfigError,
                                "Module with identifier '" << module_id << "' not found in config!"));
        }

        const std::string& module_name = config->get_main_config()[module_id]["module"].get<std::string>();
        auto module_manifest = config->get_manifests()[module_name];
        // FIXME (aw): get_classes should be called get_units and should contain the type of class for each unit
        auto module_impls = config->get_interfaces()[module_name];

        // initialize everest framework
        const auto& module_identifier = config->printable_identifier(module_id);
        EVLOG_debug << "Initializing framework for module " << module_identifier << "...";
        EVLOG_debug << "Trying to set process name to: '" << module_identifier << "'...";
        if (prctl(PR_SET_NAME, module_identifier.c_str())) {
            EVLOG_warning << "Could not set process name to '" << module_identifier << "'";
        }

        Everest::Logging::update_process_name(module_identifier);

        // connect to mqtt server and start mqtt mainloop thread
        ctx = new EvModCtx(
            Everest::Everest::get_instance(module_id, *config, validate_schema, mqtt_server_address, mqtt_server_port),
            module_manifest, env);
        ctx->everest.connect();

        ctx->js_module_ref = Napi::Persistent(module_this);

        ctx->everest.spawn_main_loop_thread();

        ctx->js_cb = std::make_unique<JsExecCtx>(env, callback_wrapper);

        //
        // fill in everything we know about the module
        //

        // provides property: iterate over every implementation that this modules provides
        auto provided_vars_prop = Napi::Object::New(env);
        auto provided_cmds_prop = Napi::Object::New(env);
        for (const auto& impl_definition : module_impls.items()) {
            const auto& impl_id = impl_definition.key();
            const auto& impl_intf = module_impls[impl_id];

            auto set_prop = Napi::Object::New(env);

            // iterator over every variable, that the interface of this implementation provides
            auto impl_vars_prop = Napi::Object::New(env);
            if (impl_intf.contains("vars")) {
                for (const auto& var_entry : impl_intf["vars"].items()) {
                    const auto& var_name = var_entry.key();

                    impl_vars_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                        var_name,
                        Napi::Function::New(env,
                                            [impl_id, var_name](const Napi::CallbackInfo& info) {
                                                return publish_var(impl_id, var_name, info);
                                            }),
                        napi_enumerable));
                }
            }

            if (impl_vars_prop.GetPropertyNames().Length()) {
                auto var_publish_prop = Napi::Object::New(env);
                var_publish_prop.DefineProperty(
                    Napi::PropertyDescriptor::Value("publish", impl_vars_prop, napi_enumerable));
                provided_vars_prop.DefineProperty(
                    Napi::PropertyDescriptor::Value(impl_id, var_publish_prop, napi_enumerable));
            }

            auto cmd_register_prop = Napi::Object::New(env);
            // iterate over every command, that the interface offers
            if (impl_intf.contains("cmds")) {
                for (const auto& cmd_entry : impl_intf["cmds"].items()) {
                    const auto& cmd_name = cmd_entry.key();
                    cmd_register_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                        cmd_name,
                        Napi::Function::New(env,
                                            [impl_id, cmd_name](const Napi::CallbackInfo& info) {
                                                return setup_cmd_handler(impl_id, cmd_name, info);
                                            }),
                        napi_enumerable));
                }
            }

            if (cmd_register_prop.GetPropertyNames().Length()) {
                auto impl_cmds_prop = Napi::Object::New(env);
                impl_cmds_prop.DefineProperty(
                    Napi::PropertyDescriptor::Value("register", cmd_register_prop, napi_enumerable));
                provided_cmds_prop.DefineProperty(
                    Napi::PropertyDescriptor::Value(impl_id, impl_cmds_prop, napi_enumerable));
            }
        }
        module_this.DefineProperty(Napi::PropertyDescriptor::Value("provides", provided_vars_prop, napi_enumerable));
        available_handlers_prop.DefineProperty(
            Napi::PropertyDescriptor::Value("provides", provided_cmds_prop, napi_enumerable));

        // uses[_optional] property
        auto uses_vars_prop = Napi::Object::New(env);
        auto uses_list_vars_prop = Napi::Object::New(env);
        auto uses_cmds_prop = Napi::Object::New(env);
        auto uses_list_cmds_prop = Napi::Object::New(env);
        for (auto& requirement : module_manifest["requires"].items()) {
            auto const& requirement_id = requirement.key();
            json req_route_list = config->resolve_requirement(module_id, requirement_id);
            // if this was a requirement with min_connections == 1 and max_connections == 1,
            // this will be simply a single connection, but an array of connections otherwise
            // (this array can have only one entry, if only one connection was provided, though)
            const bool is_list = req_route_list.is_array();
            if (!is_list) {
                req_route_list = json::array({req_route_list});
            }
            auto req_mod_vars_array = Napi::Array::New(env);
            auto req_mod_cmds_array = Napi::Array::New(env);
            for (size_t i = 0; i < req_route_list.size(); i++) {
                auto req_route = req_route_list[i];
                const std::string& requirement_module_id = req_route["module_id"];
                const std::string& requirement_impl_id = req_route["implementation_id"];
                // FIXME (aw): why is const auto& not possible for the following line?
                // we only want cmds/vars from the required interface to be usable, not from it's child interfaces
                std::string interface_name = req_route["required_interface"].get<std::string>();
                auto requirement_impl_intf = config->get_interface_definition(interface_name);
                auto requirement_vars = Everest::Config::keys(requirement_impl_intf["vars"]);
                auto requirement_cmds = Everest::Config::keys(requirement_impl_intf["cmds"]);

                auto var_subscribe_prop = Napi::Object::New(env);
                for (auto const& var_name : requirement_vars) {
                    var_subscribe_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                        var_name,
                        Napi::Function::New(
                            env,
                            [requirement_id, i, var_name](const Napi::CallbackInfo& info) {
                                return set_var_subscription_handler({requirement_id, i}, var_name, info);
                            }),
                        napi_enumerable));
                }

                auto req_mod_vars_prop = Napi::Object::New(env);
                req_mod_vars_prop.DefineProperty(
                    Napi::PropertyDescriptor::Value("subscribe", var_subscribe_prop, napi_enumerable));

                if (requirement_vars.size()) {
                    if (is_list) {
                        req_mod_vars_array.Set(i, req_mod_vars_prop);
                    } else {
                        uses_vars_prop.DefineProperty(
                            Napi::PropertyDescriptor::Value(requirement_id, req_mod_vars_prop, napi_enumerable));
                    }
                }

                auto cmd_prop = Napi::Object::New(env);
                for (auto const& cmd_name : requirement_cmds) {
                    cmd_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                        cmd_name,
                        Napi::Function::New(env,
                                            [requirement_id, i, cmd_name](const Napi::CallbackInfo& info) {
                                                return call_cmd({requirement_id, i}, cmd_name, info);
                                            }),
                        napi_enumerable));
                }

                auto req_mod_cmds_prop = Napi::Object::New(env);
                req_mod_cmds_prop.DefineProperty(Napi::PropertyDescriptor::Value("call", cmd_prop, napi_enumerable));

                if (requirement_cmds.size()) {
                    if (is_list) {
                        req_mod_cmds_array.Set(i, req_mod_cmds_prop);
                    } else {
                        uses_cmds_prop.DefineProperty(
                            Napi::PropertyDescriptor::Value(requirement_id, req_mod_cmds_prop, napi_enumerable));
                    }
                }
            }

            uses_list_vars_prop.DefineProperty(
                Napi::PropertyDescriptor::Value(requirement_id, req_mod_vars_array, napi_enumerable));
            uses_list_cmds_prop.DefineProperty(
                Napi::PropertyDescriptor::Value(requirement_id, req_mod_cmds_array, napi_enumerable));
        }

        if (uses_vars_prop.GetPropertyNames().Length())
            available_handlers_prop.DefineProperty(
                Napi::PropertyDescriptor::Value("uses", uses_vars_prop, napi_enumerable));
        if (uses_list_vars_prop.GetPropertyNames().Length())
            available_handlers_prop.DefineProperty(
                Napi::PropertyDescriptor::Value("uses_list", uses_list_vars_prop, napi_enumerable));

        if (uses_cmds_prop.GetPropertyNames().Length())
            module_this.DefineProperty(Napi::PropertyDescriptor::Value("uses", uses_cmds_prop, napi_enumerable));
        if (uses_list_cmds_prop.GetPropertyNames().Length())
            module_this.DefineProperty(
                Napi::PropertyDescriptor::Value("uses_list", uses_list_cmds_prop, napi_enumerable));

        // external mqtt property
        if (module_manifest.contains("enable_external_mqtt") && module_manifest["enable_external_mqtt"] == true) {
            auto mqtt_prop = Napi::Object::New(env);
            mqtt_prop.DefineProperty(
                Napi::PropertyDescriptor::Value("publish", Napi::Function::New(env, mqtt_publish), napi_enumerable));

            mqtt_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                "subscribe", Napi::Function::New(env, mqtt_subscribe), napi_enumerable));

            module_this.DefineProperty(Napi::PropertyDescriptor::Value("mqtt", mqtt_prop, napi_enumerable));
        }

        // config property
        json module_config = config->get_module_json_config(module_id);
        auto module_config_prop = Napi::Object::New(env);
        auto module_config_impl_prop = Napi::Object::New(env);

        for (auto& config_map : module_config.items()) {
            if (config_map.key() == "!module") {
                module_config_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                    "module", convertToNapiValue(env, config_map.value()), napi_enumerable));
                continue;
            }
            module_config_impl_prop.DefineProperty(Napi::PropertyDescriptor::Value(
                config_map.key(), convertToNapiValue(env, config_map.value()), napi_enumerable));
        }
        module_config_prop.DefineProperty(
            Napi::PropertyDescriptor::Value("impl", module_config_impl_prop, napi_enumerable));

        module_this.DefineProperty(Napi::PropertyDescriptor::Value("config", module_config_prop, napi_enumerable));

        auto module_info_prop = Napi::Object::New(env);
        // set moduleName property
        module_info_prop.DefineProperty(
            Napi::PropertyDescriptor::Value("name", Napi::String::New(env, module_name), napi_enumerable));

        // set moduleId property
        module_info_prop.DefineProperty(
            Napi::PropertyDescriptor::Value("id", Napi::String::New(env, module_id), napi_enumerable));

        // set printable identifier
        module_info_prop.DefineProperty(Napi::PropertyDescriptor::Value(
            "printable_identifier", Napi::String::New(env, module_identifier), napi_enumerable));

        module_this.DefineProperty(Napi::PropertyDescriptor::Value("info", module_info_prop, napi_enumerable));

        ctx->everest.register_on_ready_handler(framework_ready_handler);
    } catch (std::exception& e) {
        EVLOG_AND_RETHROW(env);
    }

    return available_handlers_prop;
}

const std::string extract_logstring(const Napi::CallbackInfo& info) {
    // TODO (aw): check input
    return info[0].ToString().Utf8Value();
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    BOOST_LOG_FUNCTION();

    //
    // setup logging functions
    //
    Napi::Object log = Napi::Object::New(env);

    log.DefineProperty(Napi::PropertyDescriptor::Value(
        "debug",
        Napi::Function::New(env, [](const Napi::CallbackInfo& info) { EVLOG_debug << extract_logstring(info); }),
        napi_enumerable));

    log.DefineProperty(Napi::PropertyDescriptor::Value(
        "info",
        Napi::Function::New(env, [](const Napi::CallbackInfo& info) { EVLOG_info << extract_logstring(info); }),
        napi_enumerable));

    log.DefineProperty(Napi::PropertyDescriptor::Value(
        "warning",
        Napi::Function::New(env, [](const Napi::CallbackInfo& info) { EVLOG_warning << extract_logstring(info); }),
        napi_enumerable));

    log.DefineProperty(Napi::PropertyDescriptor::Value(
        "error",
        Napi::Function::New(env, [](const Napi::CallbackInfo& info) { EVLOG_error << extract_logstring(info); }),
        napi_enumerable));

    log.DefineProperty(Napi::PropertyDescriptor::Value(
        "critical",
        Napi::Function::New(env, [](const Napi::CallbackInfo& info) { EVLOG_critical << extract_logstring(info); }),
        napi_enumerable));

    exports.DefineProperty(Napi::PropertyDescriptor::Value("log", log, napi_enumerable));

    exports.DefineProperty(
        Napi::PropertyDescriptor::Value("signal_ready", Napi::Function::New(env, signal_ready), napi_enumerable));

    exports.DefineProperty(
        Napi::PropertyDescriptor::Value("boot_module", Napi::Function::New(env, boot_module), napi_enumerable));

    return exports;
}

NODE_API_MODULE(everestjs, Init)

} // namespace EverestJs
