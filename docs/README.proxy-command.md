# Connecting through an external command

`/proxy-command:<command>` connects FreeRDP through a program's standard input
and standard output. The program must open a bidirectional byte stream to the
RDP server. It can be OpenSSH, a VPN helper, or another stream connector.

For example, on Linux or macOS:

```sh
xfreerdp /v:desktop.internal:3389 /u:alice \
  /proxy-command:'ssh -o BatchMode=yes -W %h:%p alice@bastion.example.com'
```

On Windows, the same option works with the installed OpenSSH client:

```powershell
wfreerdp.exe /v:desktop.internal:3389 /u:alice '/proxy-command:ssh.exe -o BatchMode=yes -W %h:%p alice@bastion.example.com'
```

The bastion resolves `desktop.internal` and opens the destination connection.
No local forwarding port is needed. SSH authenticates the bastion and encrypts
the client-to-bastion connection. FreeRDP still negotiates RDP security with the
actual desktop, including its TLS certificate and NLA authentication. The command
itself does not add encryption: a plain stream connector is not a secure tunnel.
SSH forwarding protects the SSH leg; use RDP TLS for the bastion-to-desktop leg.

Keep certificate checking enabled and configure SSH host-key verification.
Authenticate SSH using an existing key or agent before connecting. The command
transport is intended for noninteractive helpers; `BatchMode=yes` makes SSH fail
instead of waiting for a password or a host-key prompt. On Unix, helpers run in
a separate process group and interactive terminal reads are not supported.

## Command syntax

FreeRDP expands these tokens for each connection attempt:

- `%h`: the current RDP destination hostname.
- `%p`: the current RDP destination port.
- `%%`: a literal percent sign.

Unknown tokens and a trailing percent sign are rejected. Escape any other
percent signs in the command, including shell `printf` formats or Windows
environment references, as `%%`.

Linux and macOS execute the expanded command with `/bin/sh -c`. Windows uses
`CreateProcessW` directly; shell operators need an explicit `cmd.exe /c` or
PowerShell invocation. Quote executable paths containing spaces using the
platform's command-line syntax. Standard output must contain only connection
bytes; diagnostics belong on standard error, which FreeRDP inherits.

Target hostnames accept ASCII letters, digits, dots, underscores, colons and
hyphens, with percent signs additionally allowed on Unix for IPv6 zone IDs.
A leading hyphen, whitespace and shell metacharacters are rejected to prevent
server-provided redirection names from injecting commands. Use ASCII/punycode
hostnames. Windows target zone IDs containing `%` are rejected because explicitly
requested shells can interpret them as environment-variable syntax.

For an IPv6 target, use OpenSSH's bracketed forwarding destination syntax:

```sh
xfreerdp /v:'[2001:db8::10]:3389' \
  /proxy-command:'ssh -o BatchMode=yes -W "[%h]:%p" alice@bastion.example.com'
```

The command is trusted executable configuration. Only supply commands you intend
to run as your local user. This option is not imported from a standard `.rdp`
property; applications can set `FreeRDP_ProxyCommand` with the settings API.

## Connection behavior

Each attempt starts a new helper, including security negotiation retries,
automatic reconnect and server redirection. The logical destination remains
available to TLS/NLA; internal redirected hostnames do not need local DNS.
Normal transport I/O callbacks remain overridable by embedding applications.

FreeRDP never falls back to a direct TCP connection after a command failure.
Gateway, ARM gateway, child-session and explicit HTTP/SOCKS proxy combinations
are rejected. Environment HTTP/SOCKS proxies are bypassed. RDP UDP multitransport
is disabled because a standard input/output command carries only a byte stream.

`/timeout:<milliseconds>` (`FreeRDP_TcpConnectTimeout`, default 15000) bounds helper
startup until the first bytes are received. It is not a session idle timeout or
a complete TLS/NLA handshake timeout. Disconnect or cancellation closes the
stream and terminates the helper process group on Unix or its Windows Job Object.
Unix cleanup gives the process group a short termination grace period before
forcing termination and reaping the direct child. Programs that deliberately
detach from the process group are outside this cleanup contract.

Native Windows support requires Windows 7 or newer. Starting inside an
incompatible Windows 7 Job Object fails before the helper is resumed.
Unix requires `posix_spawn` support for closing all inherited descriptors
(`posix_spawn_file_actions_addclosefrom_np`, or macOS close-on-exec-default).
Unsupported runtimes fail explicitly. Android and WebAssembly do not support
external process transports.

## Implementation and validation

The common layer validates configuration and expands tokens. The Unix layer
uses a socket pair, `posix_spawn`, and nonblocking socket events. The Windows layer
uses two private overlapped named pipes, an explicit inherited-handle list, and
a Job Object that terminates the command's process tree when closed. Existing
OpenSSL BIO buffering, TLS and RDP negotiation operate above these layers.

Configure tests with `BUILD_TESTING=ON`. `TestProxyCommand` checks Unix binary I/O,
events, tokens, EOF, startup deadlines, cancellation and resource cleanup.
`TestProxyCommandWin` uses a native helper to test large duplex transfers,
overlapped retries, events, deadlines, cancellation and process-tree cleanup.
The common command-line and settings tests also cover the new setting.

References: [OpenSSH ProxyCommand](https://man.openbsd.org/ssh_config#ProxyCommand),
[Windows overlapped pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-overlapped-i-o),
[Windows Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
