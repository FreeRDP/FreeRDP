# Proxy filter API

`freerdp-proxy` has an API for filtering certain messages. A filter can register callbacks to events, allowing to record the data and control whether to pass/ignore the message, or right out drop the connection. 

During startup, the proxy loads its filters from the configuration:
```ini
[Filters]
; FilterName = FilterPath
DemoFilter = "server/proxy/demo.so"
```

## Currently supported events
* Mouse event
* Keyboard event

## Developing a new filter
* Create a new file that includes `filters_api.h`.
* Implement the `filter_init` function and register the callbacks you are interested in.
* Each callback receives two parameters:
    * `connectionInfo* info` holds connection info of the raised event.
    * `void* param` holds the actual event data. It should be casted by the filter to the suitable struct from `filters_api.h`.
* Each callback must return a `PF_FILTER_RESULT`:
    * `FILTER_IGNORE`: The event will not be proxied.
    * `FILTER_PASS`: The event will be proxied.
    * `FILTER_DROP`: The entire connection will be dropped.

A demo can be found in `filter_demo.c`.