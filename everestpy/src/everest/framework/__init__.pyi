from pathlib import Path
from typing import Callable, overload


class ModuleInfoPaths:
    @property
    def etc(self) -> Path: ...

    @property
    def libexec(self) -> Path: ...

    @property
    def share(self) -> Path: ...


class Interface:
    @property
    def commands(self) -> list[str]: ...

    @property
    def variables(self) -> list[str]: ...


class ModuleInfo:
    @property
    def authors(self) -> list[str]: ...

    @property
    def license(self) -> str: ...

    @property
    def id(self) -> str: ...

    @property
    def name(self) -> str: ...

    @property
    def paths(self) -> ModuleInfoPaths: ...


class ModuleSetupConfigurations:
    @property
    def implementations(self) -> dict[str, dict]: ...

    @property
    def module(self) -> dict: ...


class ModuleSetup:
    @property
    def configs(self) -> ModuleSetupConfigurations: ...

    @property
    def connections(self) -> dict[str, list[Fulfillment]]: ...


class Fulfillment:
    @property
    def module_id(self) -> str: ...

    @property
    def implementation_id(self) -> str: ...


class RuntimeSession:
    @overload
    def __init__(self) -> None: ...

    @overload
    def __init__(self, prefix: str, config_file_path: str) -> None: ...


class Module:
    @overload
    def __init__(self, session: RuntimeSession) -> None: ...

    @overload
    def __init__(self, module_id: str, session: RuntimeSession) -> None: ...

    def say_hello(self) -> ModuleSetup: ...

    @overload
    def init_done(self) -> None: ...

    @overload
    def init_done(self, on_ready_handler: Callable[[], None]) -> None: ...

    def call_command(self, fulfillment: Fulfillment,
                     command_name: str, args: dict) -> None: ...

    def publish_variable(self, implementation_id: str,
                         variable_name: str, value: dict) -> None: ...

    def implement_command(self, implementation_id: str, command_name: str,
                          handler: Callable[[dict], dict]) -> None: ...
    def subscribe_variable(self, fulfillment: Fulfillment,
                           variable_name: str, callback: Callable[[dict], None]) -> None: ...

    @property
    def requirements(self) -> dict[str, Interface]: ...

    @property
    def implementations(self) -> dict[str, Interface]: ...

    @property
    def info(self) -> ModuleInfo: ...
