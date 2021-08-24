# Proxy module API

`freerdp-proxy` has an API for hooking/filtering certain events/messages.
A module can register callbacks to events, allowing to record the data and control whether to pass/ignore, or right out drop the connection.

During startup, the proxy reads its modules from the configuration:

```ini
[Plugins]
Modules = demo,cap
```

These modules are loaded in a best effort manner. Additionally there is a configuration field for modules that must be loaded,
so the proxy refuses to start if they are not found:

```ini
[Plugins]
Required = demo,cap
```

Modules must be installed as shared libraris in the `<base install>/lib/freerdp3/proxy` folder and match the pattern
`proxy-<name>-plugin.<ext>` (e.g. `proxy-demo-plugin.so`) to be found.
For security reasons loading by full path is not supported and only the installation path is used for lookup.

## Currently supported hook events

### Client

* ClientInitConnect:     Called before the client tries to open a connection
* ClientUninitConnect:   Called after the client has disconnected
* ClientPreConnect:      Called in client PreConnect callback
* ClientPostConnect:     Called in client PostConnect callback
* ClientPostDisconnect:  Called in client PostDisconnect callback
* ClientX509Certificate: Called in client X509 certificate verification callback
* ClientLoginFailure:    Called in client login failure callback
* ClientEndPaint:        Called in client EndPaint callback

### Server

* ServerPostConnect:     Called after a client has connected
* ServerPeerActivate:    Called after a client has activated
* ServerChannelsInit:    Called after channels are initialized
* ServerChannelsFree:    Called after channels are cleaned up
* ServerSessionEnd:      Called after the client connection disconnected

## Currently supported filter events

* KeyboardEvent:         Keyboard event, e.g. all key press and release events
* MouseEvent:            Mouse event, e.g. mouse movement and button press/release events
* ClientChannelData:     Client static channel data
* ServerChannelData:     Server static channel data
* DynamicChannelCreate:  Dynamic channel create
* ServerFetchTargetAddr: Fetch target address (e.g. RDP TargetInfo)
* ServerPeerLogon:       A peer is logging on

## Developing a new module
* Create a new file that includes `freerdp/server/proxy/proxy_modules_api.h`.
* Implement the `proxy_module_entry_point` function and register the callbacks you are interested in.
* Each callback receives two parameters:
    * `connectionInfo* info` holds connection info of the raised event.
    * `void* param` holds the actual event data. It should be casted by the filter to the suitable struct from `filters_api.h`.
* Each callback must return a `BOOL`:
    * `FALSE`: The event will not be proxied.
    * `TRUE`: The event will be proxied.

A demo can be found in `filter_demo.c`.
