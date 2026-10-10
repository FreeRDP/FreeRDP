# lsa4posix — per-user LSA daemon for Remote Credential Guard

`lsa4posix` is a per-user daemon that provides the backend of the WinPR LSA
authentication-package API on non-Windows platforms. It lets FreeRDP use
[MS-CSSP] Remote Credential Guard (`/remoteGuard`) the same way it does on
Windows: the FreeRDP client talks to the daemon through the WinPR `Lsa*` client
API, and the daemon runs the CredSSP/TSSSP exchange and services the redirected
[MS-RDPEAR] Kerberos calls on the client's behalf. The FreeRDP process itself
holds no Kerberos TGT and no CredSSP key.

It is built with `-DWITH_LSA4POSIX=ON` and requires MIT Kerberos
(`-DWITH_KRB5=ON`, which also pulls in the `rdpear` channel). It is MIT-only;
Heimdal is not supported.

## Running

The daemon is one process per user. It can be started manually or as a systemd
user service.

Manually (foreground, for a first look):

```
lsa4posix --foreground --debug
```

Options:

- `--socket PATH` — listen on `PATH` instead of the default location.
- `--foreground` — stay in the foreground instead of daemonizing.
- `--debug` — raise every WinPR logger to `TRACE`.

### Socket location

The client and daemon rendezvous on an `AF_UNIX` socket, chosen in this order:

1. `$LSA4POSIX_SOCKET`
2. `$XDG_RUNTIME_DIR/lsa4posix/lsa.sock`
3. `$TMPDIR/lsa4posix-<uid>/lsa.sock` (macOS)
4. `/tmp/lsa4posix-<uid>/lsa.sock`

The parent directory is created `0700` and owned by the invoking user; the
client verifies the owner and mode before connecting, and the daemon checks the
peer's uid (`SO_PEERCRED`/`LOCAL_PEERCRED`) on every connection and rejects any
peer whose uid differs from its own. The threat model is the same as
`ssh-agent`: the socket is private to the user.

### systemd (user)

Install `packaging/lsa4posix.socket` and `packaging/lsa4posix.service` into
`~/.config/systemd/user/`, then enable socket activation:

```
systemctl --user enable --now lsa4posix.socket
```

The daemon is then started on demand on the first connection, with the listening
socket passed to it by systemd (the `LISTEN_FDS` protocol). It also runs
standalone (`lsa4posix --foreground`), binding its own socket, when not
socket-activated.

### macOS (launchd)

Install `packaging/com.freerdp.lsa4posix.plist` into `~/Library/LaunchAgents/`
and load it with `launchctl load`.

## Credentials

Remote Credential Guard forwards a Kerberos ticket from the user's credential
cache, so a **forwardable** TGT must be present before connecting:

```
kinit -f user@REALM
xfreerdp /v:<host> /remoteGuard /u:user /d:domain
```

The daemon uses the existing credential cache (`krb5_cc_default`, including KCM
and keyring caches) for single sign-on. If the TGT is not forwardable the logon
is refused; obtain one with `kinit -f`.

## Logging

The daemon logs under the `com.winpr.lsa4posix` WLog tag. Use `--debug` for
`TRACE`. Only identifiers, message types, lengths and statuses are logged;
secrets are zeroed after use.
