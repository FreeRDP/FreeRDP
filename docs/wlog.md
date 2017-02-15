# Overview

WLog is a configurable and flexible logging system used throughout winpr and
FreeRDP.

The primary concept is to have a hierarchy of loggers that can be be configured
independently.

TODO add more details and configuration examples.



# Environment variables

* WLOG_APPENDER  - the appender to use possible values below also see the Appender section.
  * CONSOLE
  * FILE
  * BINARY
  * SYSLOG
  * JOURNALD
  * UDP
* WLOG_PREFIX - configure the prefix used for outputting the message (see
  Format for more details and examples)
* WLOG_LEVEL - the level to output messages for
* WLOG_FILTER - sets a filter for WLog messages. Only the filtered messages are
printed
* WLOG_FILEAPPENDER_OUTPUT_FILE_PATH  - set the output file path for the file
file appender
* WLOG_FILEAPPENDER_OUTPUT_FILE_NAME - set the output file name for the output
appender
* WLOG_JOURNALD_ID - identifier used by the journal appender
* WLOG_UDP_TARGET - target to use for the UDP appender in the format host:port

# Levels

The WLog are complementary the higher level always includes the lower ones.
The level list below is top down. Top the highest level.

* WLOG_TRACE - print everything including package dumps
* WLOG_DEBUG - debug messages
* WLOG_INFO - general informations
* WLOG_WARN - warnings
* WLOG_ERROR - errors
* WLOG_FATAL - fatal problems
* WLOG_OFF - completely disable the wlog output


# Format

The format a logger prints in has the following possible options:

* "lv" - log level
* "mn" - module name
* "fl" - file name
* "fn" - function
* "ln" - line number
* "pid" - process id
* "tid" - thread id
* "yr" - year
* "mo" - month
* "dw" - day of week
* "hr" - hour
* "mi" - minute
* "se" - second
* "ml" - millisecond

A maximum of 16 options can be used per format string.

An example that generally sets the WLOG_PREFIX for xfreerdp would look like:
```
WLOG_PREFIX="pid=%pid:tid=%tid:fn=%fn -" xfreerdp /v:xxx
```

# Appenders

WLog uses different appenders that define where the log output should be written
to. If the application doesn't explicitly configure the appenders the above
described variable WLOG_APPENDER can be used to choose one appender.

The following represents an overview about all appenders and their possible
configuration values.

### Binary

Write the log data into a binary format file.

Options:
* "outputfilename", value const char* - file to write the data to
* "outputfilepath", value const char* - location of the output file

### Callback
The callback appender can be used from an application to get all log messages
back the application. For example if an application wants to handle the log
output itself.

Options:

* "callbacks", value struct wLogCallbacks*, callbacks to use

### Console

The console appender writes to the console. Depending of the operating system
the application runs on the output might be handled differently. For example
on android log print would be used.

Options:


* "outputstream", value const char * - output stream to write to
  * "stdout" - write everything to stdout
  * "stderr" - write everything to stderr
  * "default" - use the default settings - in this case errors and fatal would
  go to stderr everything else to stdout
  * debug - use the debug output. Only used on windows on all operating systems
  this behaves as as if default was set.

### File
The file appender writes the textual output to a file.

Options:

* "outputfilename", value const char*, filename to use
* "outputfilepath", value const char*, location of the file

### Udp

This appender sends the logging messages to a pre-defined remote host via UDP.

Options:

* "target", value const char*, target to send the data too in the format
host:port

If no target is set the default one 127.0.0.1:20000 is used. To receive the
log messages one can use netcat. To receive the default target the following
command could be used.
```
nc -u 127.0.0.1 -p 20000 -l
```

### Syslog (optional)

Use syslog for outputting the debug messages. No options available.

### Journald (optional)

For outputting the log messages to journald this appender can be used.
The available options are:

* "identifier", value const char*, the identifier to use for journald (default
  is winpr)
