# Commit message syntax guidelines

When tracking down problems, just looking at the code history, when trying to pinpoint the difference
between two branches/tags/releases or for automatic changelog generation proper and "good" commit messages are important
and can be a blessing.

Therefore we use the following conventions for commit messages for all ogon projects.

## Syntax

A commit message consists of a **header**, **body** and **footer**.

```
<header>
<BLANK LINE>
<body>
<BLANK LINE>
<footer>
```

### Header
The header is the first line and should in the following format:

```
[<scope>,<sub-scope>] <imperative, lowercase summary>
```

- Scope tag: one to three comma-separated tokens in square brackets, with no spaces, broadest area first. Don’t write `[client, ios]`; write `[client,ios]`.
- Summary: imperative mood, starting lowercase, with no trailing period. Examples: fix, add, drop, use, limit, handle, clamp, improve, revert.
- Length: aim for 72 characters or fewer, including the tag.
- Identifiers: keep code identifiers in their exact case, for example [winpr,json] return a copy from WINPR_JSON_Print*.
- Untagged titles: avoid titles like Video channel fix or Sdl inhibit option. 
  - "Merge .." - identifies a merge (e.g. when a pull request was merged)

Example:

```
[channels,client] fix endless loop in getFromGeneric
```


The type shouldn't be capitalized (with the exception of Merge).

#### Scopes

| scope | subs-scope |
|---|---|
| `core` | `rdp`, `settings`, `gateway`, `connection`, `caps`, `update`, `rdstls`, `proxy`, `utils` |
| `client` | `sdl`, `sdl3`, `x11`, `ios`, `common` |
| `winpr` | `crt`, `crypto`, `smartcard`, `kerberos`, `ntlm`, `json`, `input`, `platform`, `wlog`, `utils` |
| `codec` | `h264`, `nsc`, `progressive`, `video`, `fdk` |
| `channels` | `audin`, `video`, `rdpewa` |
| non-code | `packaging,flatpak`, `github,templates`, `ci`, `build`, `refactor`,  `formatting` |

Scope names should be lowercase (`x11`, not `X11`).

### Body

The body is a free form multi-line text that should be wrapped at a length of 72 characters.
Like the header the body should be written in imperative present tense and should contain at least the following:

- a meaningful message
- a description of problem that is addressed (what's wrong with the current code)
- how the problem was solved an why it was done this way
- in case of performance improvements measurements before and after the change for comparison
- (what other solutions were discussed/possible)

For small commits, or commits which do not need further description the body can be omitted. Examples for this are
style changes, typo fixes.

### Footer

The footer is optional. As the body it should be wrapped at a length of 72 characters.
It can be used to reference issues on GitHub that this commit closes like

```
Fixes #13622
```

Multiple issues can be referenced at once see "[closing issues using keyword](https://help.github.com/articles/closing-issues-using-keywords/)" for details.

If any changes are introduced that can "break" something, add or remove new configuration settings, add or update dependencies or require additional manual steps this should be noted in the footer like:
```
BREAKS: foo bar faz
```
or
```
CONFIG: remove obsolete option xyz
```
or
```
DEPENDENCY: require latest version (xyz) of abcde
```

## Granularity

- One logical change per commit.
- Keep formatting-only or warning-only cleanups in their own commits, with tags like `[warnings] ...`.
- Ideally don't mix unrelated subsystems in one commit. If you must touch several, list them in the tag, e.g. `[core,client]`.

## Examples

```
[client,sdl] fix mouse and touchpad natural scroll setting being ignored

[codec,progressive] fix underflow guard in progressive_rfx_quant_sub

[winpr,crypto] fix RC4 FIPS override for OpenSSL 3

The OpenSSL 3 provider rejects RC4 when FIPS mode is active, so
fall back to the internal implementation for the non-critical RDP
legacy path. Fixes #1234.
```

## Known inconsistencies in existing history

- Older commits use `wlog: ...`, `cmake: ...` or `winpr/argv: ...` instead of brackets.
- Case varies, e.g. `x11` vs `X11`.
- Some titles skip the tag entirely.
