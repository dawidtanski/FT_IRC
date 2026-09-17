*This project has been created as part of the 42 curriculum by dtanski, kjamrosz.*

# ft_irc

## Description

ft_irc is a standalone Internet Relay Chat **server** written in C++98 for the
42 curriculum. It lets multiple clients authenticate, join channels, exchange
private and channel messages, and manage channels through operator commands.
The implementation targets the mandatory requirements of ft_irc subject v10.0.
It does not implement server-to-server networking or the optional bot/file-transfer bonuses.

The server uses TCP and one `poll()` loop. Listening and accepted sockets are
non-blocking. Each client has an input buffer for incomplete IRC lines and a
bounded output queue. `recv()` is called after `POLLIN`; `send()` is called after
`POLLOUT`. Partial writes remain queued for a later writable event, so a slow
receiver cannot block everyone else. No external library or worker process is
used by the server.

## Instructions

### Build and run

Requirements: a POSIX environment, Make, and a C++ compiler available as `c++`.
No installation or configuration file is needed.

```sh
make
./ircserv 6667 secret
```

The executable accepts exactly two arguments:

- `port`: a decimal number from 1 to 65535; choose an available, unprivileged port.
- `password`: the connection password, 1–128 characters without whitespace.

The listener binds to an available wildcard address returned by `getaddrinfo()`.
Clients on other machines can connect through the server machine's address.
Connections use plain TCP; TLS and SASL are not implemented. Stop the server with
`Ctrl+C` or `SIGTERM`; sockets and client state are released on shutdown.

```sh
make clean    # Remove objects and dependency files
make fclean   # Also remove ircserv
make re       # Rebuild from scratch
```

Compilation uses `-Wall -Wextra -Werror -std=c++98`. Header dependencies are
tracked, and an unchanged build does not relink.

### Reference client: Irssi

Irssi is the reference client for this implementation. Install it separately
using your system's package manager, then connect with a unique nickname:

```sh
irssi --noconnect --connect=127.0.0.1 --port=6667 --password=secret --nick=alice
```

In another terminal, launch a second instance with a separate configuration:

```sh
irssi --home=/tmp/ft-irc-bob --noconnect --connect=127.0.0.1 --port=6667 --password=secret --nick=bob
```

Inside both clients:

```text
/join #general
/msg #general Hello everyone
/msg bob Hello Bob
/nick alice2
/part #general
/quit Goodbye
```

Use a nickname other than the recipient's when testing private messages. The
first user to create a channel becomes its operator, displayed with `@`.
Existing operators can grant that role to other members.

### Channel commands

| Command | Example | Behavior |
| --- | --- | --- |
| `JOIN` | `/join #general` | Join or create a channel |
| `PART` | `/part #general Leaving` | Leave a channel |
| `KICK` | `/kick #general bob Reason` | Operator removes a member |
| `INVITE` | `/invite bob #general` | Invite a user; requires an operator on an invite-only channel |
| `TOPIC` | `/topic #general A new topic` | Set a topic; omit the text to query it |
| `MODE +i / -i` | `/mode #general +i` | Enable/disable invite-only entry |
| `MODE +t / -t` | `/mode #general +t` | Restrict topic changes to operators / allow members |
| `MODE +k / -k` | `/mode #general +k key` | Set/remove the channel password |
| `MODE +o / -o` | `/mode #general +o bob` | Grant/revoke a member's operator role |
| `MODE +l / -l` | `/mode #general +l 10` | Set/remove the member limit |

Use `/join #general key` for a protected channel and `/mode #general -k key` to
remove its key. The server also accepts `-k` without the old key. Combined modes
such as `/mode #general +itkl key 10` are supported.

At the wire level, `JOIN #one,#two` and `PART #one,#two` accept comma-separated
channels, and `PRIVMSG alice,bob :Hello` accepts multiple recipients.
`JOIN 0` leaves all joined channels. Empty channels are deleted. Invitations are
single-use and follow the invited connection across nickname changes; they are
removed on disconnect.

The server also handles `PING`/`PONG`, `NOTICE`, `AWAY`, basic `CAP` negotiation,
`MOTD`, `NAMES`, channel `WHO`, `WHOIS`, and `LIST` for client interoperability.
CAP advertises no optional capabilities. This is the required IRC subset with
supporting queries, not a complete implementation of every IRC extension.

### Limits and source layout

IRC lines are limited to 512 bytes including CRLF. Nicknames are limited to
9 characters, channel names to 50, channel keys to 50, and topics to 300.
Nickname and channel lookup use RFC1459 case mapping. There is a limit of
1024 connected clients, 50 joined channels per client, and 256 KiB of pending
output per client. A receiver exceeding its output limit is disconnected.
Oversized input lines close the offending connection. Allocation failures during
client handling trigger cleanup of the affected connection without allocating
additional cleanup state.

- `main.cpp`: argument validation and startup.
- `src/Server.cpp`: event loop, registration and command handling.
- `src/Client.cpp`: client identity, membership and input/output buffers.
- `src/Channel.cpp`: members, operator roles, modes, topics and invitations.
- `src/Parser.cpp`: IRC message syntax and parameter parsing.
- `src/utils.cpp`: socket-address and case-mapping helpers.

## Resources

- [RFC 2812 — IRC Client Protocol](https://www.rfc-editor.org/rfc/rfc2812): message syntax, registration, commands and numeric replies.
- [poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html): readiness notifications and error events.
- [Irssi documentation](https://irssi.org/documentation/): reference-client usage.
- The supplied **ft_irc subject v10.0**: mandatory scope, permitted functions, evaluation and README requirements.

### Use of AI

We have used AI to clarify certain concepts and RFC guidelines, as well as to support the planning of our work.