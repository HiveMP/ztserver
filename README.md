# ztserver

![Build Status](https://build-oss.redpoint.games/buildStatus/icon?job=HiveMP/ztserver/master)

_A local ZeroTier daemon for forwarding UDP traffic over ZeroTier, controllable via REST API._

ztserver provides user-space networking over ZeroTier to desktop applications, without linking directly against libzt. This means that your applications can communicate over a ZeroTier network:

- Using the standard system functions like `recvfrom` and `sendto`, without using the `zts_` variants.
- From other languages like Node.js or C# without requiring bindings or ABI compatibility.
- From user-space, without requiring a commercial embedding license from ZeroTier, Inc. By launching ztserver as it's own process, your application avoids linking against the GPL licensed code of libzt.

ztserver is still experimental and is not recommended for production usage.

## Download

Currently ztserver is only available as source code; we are not yet automatically building and publishing compiled binaries.

## Launching

ztserver launches a HTTP server listening on port 8080. Currently the port is not configurable, but will be a command line option in the future.

## REST API

Most REST APIs return a standard response format, where the result indicates whether the operation was a success, and if not, the associated error message.

For successful responses, the result will be:

```json
{
  "outcome": true
}
```

For error responses, the result will be:

```json
{
  "outcome": false,
  "error": "error message"
}
```

### PUT /join

Connects to the ZeroTier network based on `nwid`. Call this once after launching the `ztserver` executable. If you need to join multiple networks, or if you need to rejoin a network after calling `PUT /leave` later, you'll need to launch multiple `ztserver` processes.

**Request:**

```json
{
  "nwid": "<zerotier network id>",
  "path": "path/to/store/identity/files"
}
```

**Response:** Standard format (see above)

### PUT /forward

Creates a local port which bi-directionally forwards traffic into the ZeroTier network.

This accepts a local UDP port number and optionally a proxy UDP port number. With a proxy port, you can send UDP IPv4 and IPv6 traffic to any destination in the ZeroTier network.

The local UDP port number is where incoming traffic is forwarded to - that is, when messages arrive at the ZeroTier interface of ztserver in the network, ztserver will prepend the sender information to the message and forward it to the local port for your application to receive.

The proxy UDP port number is where you send outgoing traffic from your application. You prepend the real destination to the buffer before you send the UDP packet, then you send it to this port number on localhost. If you don't provide `proxyport` in the request, ztserver will find a free UDP port on localhost and use that.

See "Messaging Format" below for details on how sender and receiver information is encapsulated in messages.

**Request:** 

```json
{
  "localport": 57571
}
```

or

```json
{
  "localport": 57571,
  "proxyport": 12000
}
```

**Response:** Either the standard error format (with `outcome` set to `false`), or the following structure:

```json
{
  "proxyport": 12345
}
```

### PUT /unforward

Removes a local port that is currently forwarding bi-directional traffic into ZeroTier; a port that was previously set up with `/forward`.

**Request:** 

```json
{
  "proxyport": 12345
}
```

**Response:** Standard format (see above)

### PUT /shutdown

Prepares `ztserver` for shutdown, disconnecting from the ZeroTier network. After getting a successful response from this REST API, you should send `SIGTERM` or the Windows equivalent to the `ztserver` process in order to stop it.

You will need to relaunch `ztserver` to connect to a network again after calling this method, as `PUT /join` will not work a second time.

**Request:**

```json
{
}
```

**Response:** Standard format (see above)

### GET /info

Returns information about the current ZeroTier connection.

**Request:** No request body.

**Response:** Either the standard error format (with `outcome` set to `false`), or the following structure:

```json
{
  "path": "path/to/store/identity/files",
  "nwid": "<zerotier network id>",
  "nodeid": "<zerotier node id>",
  "addresses": [
    "1.1.1.1",
    "1:1:1:1:1:1",
    "... etc, IPv4 or IPv6 addresses ..."
  ]
}
```

## Messaging Format

In order to send and receive messages to multiple hosts within ZeroTier from a single local UDP port, the real sender and receiver information has to be encapsulated as a header on the UDP data that's being sent.

For inbound messages being sent to the local port (received from the ZeroTier network) and for outbound messages being sent to the proxy port (sent to the ZeroTier network), they have the following format:

```
1 byte            - Address Type (0x00 for IPv4, 0x01 for IPv6)
4 bytes for IPv4, 
16 bytes for IPv6 - Address in ZT network
2 bytes           - UDP port in ZT network (unsigned short)
...               - Actual data to send
```

The header length varies depending on whether you're sending data to an IPv4 or IPv6 address. Immediately following the "UDP port in ZT network" is the actual data to be sent or actual data received over UDP.

Therefore when receiving a message on your local UDP port, you should read the first byte to determine the header length, then separate the header data from the actual message data. When sending a message to the proxy UDP port, you need to build the header based on where you want to send the data in the ZeroTier network, then prefix the header onto the data before sending it to the proxy UDP port.

## License

`ztserver` is effectively GPL licensed because it links against `libzt` (which is GPL licensed), and statically links against `ulfius` which is LGPL licensed.

That said, the `main.cpp` file itself is provided under the MIT license, so if you want to use the code in that file independent of the dependencies, you are able to do so with the terms of the MIT license.