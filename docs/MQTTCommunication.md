# EVerest Framework MQTT Topic Structure

This document describes the MQTT topic structure used by the EVerest framework for communication between modules.

## Topic Prefix Structure

The EVerest framework uses a configurable MQTT prefixes for topics. This allows multiple instances of EVerest
to run at the same time using the same broker. The default prefix is `everest`.

## 1. Variables (Vars)

The following structure applies for variable topics:

### Topic Structure

```bash
{everest_prefix}modules/{module_id}/impl/{impl_id}/var/{var_name}
```

### Message Payload Structure

The payload contains the actual variable data in the `data` field.

```json
{
  "data": <variable_value>,
  "msg_type": "Var"
}
```

## 2. Commands (Cmds)

The following structure applies for command topics. Modules that provide (implement) the command subscribe to the command topic, while modules that call the command publish to it. Each command call generates a unique UUID as the call ID. The origin field identifies the calling module. Command handlers process the request and publish responses to a separate response topic.

### Topic Structure

```bash
{everest_prefix}modules/{module_id}/impl/{impl_id}/cmd/{cmd_name}
```

### Message Payload Structure

```json
{
  "data": {
    "id": "<unique_call_id>",
    "args": {
      "arg1": "value1",
      "arg2": "value2"
    },
    "origin": "<calling_module_id>"
  },
  "msg_type": "Cmd"
}
```

## 3. Command Responses

The following structure applies for command response topics. These are used to send back results or errors from command handlers to the calling module. Response topics include the calling module ID for proper routing. The call ID matches the original command request.

### Topic Structure

```bash
{everest_prefix}modules/{module_id}/impl/{impl_id}/cmd/{cmd_name}/response/{calling_module_id}
```

### Message Payload Structure

#### Successful Response

```json
{
  "data": {
    "id": <matching_call_id>,
    "origin": <responding_module_id>,
    "retval": <return_value>
  },
  "msg_type": "CmdResult"
}
```

#### Error Response

```json
{
  "data": {
    "id": <matching_call_id>,
    "origin": <responding_module_id>,
    "error": <error_value>
  },
  "msg_type": "CmdResult"
}
```

### Error Types

Command responses can contain the following error events:

- `MessageParsingFailed`: JSON parsing error
- `SchemaValidationFailed`: Schema validation error  
- `HandlerException`: Exception in command handler
- `Timeout`: Command execution timeout
- `Shutdown`: System shutdown
- `NotReady`: Module not ready

## 4. Errors

The following structure applies for error topics. Errors are raised by modules and can be subscribed to by other modules. Each error has a unique UUID and includes information about the originating module, implementation, EVSE, and connector if applicable.

### Topic Structure

```bash
{everest_prefix}modules/{module_id}/impl/{impl_id}/error/{error_namespace}/{error_type}
```

### Message Payload Structure

```json
{
  "data": {
    "description": "<error_description>",
    "message": "<error_message>",
    "origin": {
      "implementation_id": "<originating_impl>",
      "mapping": {
        "evse": 1
      },
      "module_id": "<originating_module>"
    },
    "severity": "<error_severity>",
    "state": "<error_state>",
    "sub_type": "<error_sub_type>",
    "timestamp": "<iso_timestamp>",
    "type": "<error_type>",
    "uuid": "<unique_error_id>",
    "vendor_id": "<vendor_name>"
  },
  "msg_type": "RaiseError"
}
```

