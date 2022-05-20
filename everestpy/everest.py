import importlib
from pathlib import Path
from signal import SIGINT, signal
import sys
from threading import Event
from os import environ
import everestpy

log = everestpy.log

# make sure module path is available and import module
module_path = Path(environ.get("EV_PYTHON_MODULE"))
sys.path.append(module_path.parent.resolve().as_posix())
sys.path.append(module_path.parent.parent.resolve().as_posix())
module = importlib.import_module(f"{module_path.parent.name}.module")

module_adapter_ = None
setup = None
pub_cmds = None
wait_for_exit = Event()


def sigint_handler(_signum, _frame):
    wait_for_exit.set()


signal(SIGINT, sigint_handler)


def register_module_adapter(module_adapter):
    global module_adapter_
    module_adapter_ = module_adapter


def register_everest_register(_connections):
    vec = []
    if not pub_cmds:
        return vec

    for impl_id, cmds in pub_cmds.items():
        for cmd_name, _info in cmds.items():
            handler_function = getattr(module, f"{impl_id}_{cmd_name}")

            def unpack_json_arguments(json_args):
                # TODO: check if return values need to be packed into json first
                return handler_function(**json_args)
            cmd = everestpy.EverestPyCmd()
            cmd.impl_id = impl_id
            cmd.cmd_name = cmd_name
            cmd.handler = unpack_json_arguments

            vec.append(cmd)

    return vec


def wrapped_function(cmd_with_args):
    json_args = {}
    if cmd_with_args.arguments:
        for arg in cmd_with_args.arguments:
            json_args[arg] = None

    def kwarg_cmd(_first_arg, *args, **kwargs):
        json_args_it = iter(json_args)
        for arg in args:
            key = next(json_args_it)
            json_args[key] = arg
        for key, arg in kwargs.items():
            json_args[key] = arg
        return cmd_with_args.cmd(json_args)['retval']
    cmd = kwarg_cmd
    return cmd


def register_pre_init(reqs):
    global pub_cmds
    pub_cmds = reqs.pub_cmds
    module_setup = {}
    module_interface = {}
    for k, v in reqs.vars.items():
        variables = {}
        for kk, vv in v.items():
            variables[f"subscribe_{kk}"] = vv
        InternalType = type(f"r_{k}", (object, ), variables)
        module_interface[f"r_{k}"] = variables

    for k, v in reqs.call_cmds.items():
        cmds = {}
        for kk, vv in v.items():
            cmds[f"call_{kk}"] = wrapped_function(vv)
        if f"r_{k}" in module_interface:
            module_interface[f"r_{k}"] = {**module_interface[f"r_{k}"], **cmds}
        else:
            module_interface[f"r_{k}"] = cmds

    for k, v in module_interface.items():
        InternalType = type(f"{k}", (object, ), v)
        module_setup[f"{k}"] = InternalType()

    for k, v in reqs.pub_vars.items():
        variables = {}
        for kk, vv in v.items():
            variables[f"publish_{kk}"] = vv
        InternalType = type(f"r_{k}", (object, ), variables)
        module_setup[f"p_{k}"] = InternalType()

    if reqs.enable_external_mqtt:
        mqtt_functions = {
            "publish": module_adapter_.ext_mqtt_publish,
            "subscribe": module_adapter_.ext_mqtt_subscribe
        }
        MQTT = type("mqtt", (object, ), mqtt_functions)
        module_setup["mqtt"] = MQTT()

    Setup = type("Setup", (object, ), module_setup)
    global setup
    setup = Setup()


def register_init(_module_configs, _module_info):
    module.pre_init(setup)
    module.init()


def register_ready():
    global module_adapter_
    module.ready()


if __name__ == '__main__':
    EV_MODULE = environ.get('EV_MODULE')
    EV_MAIN_DIR = environ.get('EV_MAIN_DIR')
    EV_CONFIGS_DIR = ""
    EV_SCHEMAS_DIR = environ.get('EV_SCHEMAS_DIR')
    EV_MODULES_DIR = environ.get('EV_MODULES_DIR')
    EV_INTERFACES_DIR = environ.get('EV_INTERFACES_DIR')
    EV_LOG_CONF_FILE = environ.get('EV_LOG_CONF_FILE')
    EV_CONF_FILE = environ.get('EV_CONF_FILE')

    everestpy.register_module_adapter_callback(register_module_adapter)
    everestpy.register_everest_register_callback(register_everest_register)
    everestpy.register_init_callback(register_init)
    everestpy.register_pre_init_callback(register_pre_init)
    everestpy.register_ready_callback(register_ready)

    everestpy.init(EV_MAIN_DIR, EV_CONFIGS_DIR, EV_SCHEMAS_DIR, EV_MODULES_DIR,
                   EV_INTERFACES_DIR, EV_LOG_CONF_FILE, EV_CONF_FILE, False, EV_MODULE)

    wait_for_exit.wait()
