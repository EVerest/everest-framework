import importlib.util
from signal import SIGINT, signal
from threading import Event
from os import environ
import everestpy

log = everestpy.log


spec = importlib.util.spec_from_file_location(
    'everest.module', f'{environ.get("EV_PYTHON_MODULE")}')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

module_adapter_ = None
setup = None
pub_cmds = None
wait_for_exit = Event()


def sigint_handler(signum, frame):
    wait_for_exit.set()


signal(SIGINT, sigint_handler)


def register_module_adapter(module_adapter):
    global module_adapter_
    module_adapter_ = module_adapter


def register_everest_register(connections):
    global pub_cmds

    vec = []
    if not pub_cmds:
        return vec

    for impl_id, cmds in pub_cmds.items():
        log.error(f"IMPL: {impl_id}, CMDS: {cmds}")
        for cmd_name, info in cmds.items():
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

    def kwarg_cmd(first_arg, *args, **kwargs):
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
    requirements = {}
    requirements2 = {}  # TODO: better name...
    for k, v in reqs.vars.items():
        vars = {}
        for kk, vv in v.items():
            vars[f"subscribe_{kk}"] = vv
        InternalType = type(f"r_{k}", (object, ), vars)
        requirements2[f"r_{k}"] = vars

    for k, v in reqs.call_cmds.items():
        cmds = {}
        for kk, vv in v.items():
            cmds[f"call_{kk}"] = wrapped_function(vv)
        if f"r_{k}" in requirements2:
            requirements2[f"r_{k}"] = {**requirements2[f"r_{k}"], **cmds}
        else:
            requirements2[f"r_{k}"] = cmds

    for k, v in requirements2.items():
        InternalType = type(f"{k}", (object, ), v)
        requirements[f"{k}"] = InternalType()

    for k, v in reqs.pub_vars.items():
        vars = {}
        for kk, vv in v.items():
            vars[f"publish_{kk}"] = vv
        InternalType = type(f"r_{k}", (object, ), vars)
        requirements[f"p_{k}"] = InternalType()

    global module_adapter_
    if reqs.enable_external_mqtt:
        mqtt_functions = {
            "publish": module_adapter_.ext_mqtt_publish,
            "subscribe": module_adapter_.ext_mqtt_subscribe
        }
        MQTT = type(f"mqtt", (object, ), mqtt_functions)
        requirements["mqtt"] = MQTT()
    requirements["__init__"] = lambda x: log.error(
        "constructor of Setup() called")

    Setup = type("Setup", (object, ), requirements)
    global setup
    setup = Setup()


def register_init(module_configs, module_info):
    global setup
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
