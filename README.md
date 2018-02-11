# ztserver

_A local ZeroTier daemon for forwarding UDP traffic over ZeroTier, controllable via REST API._

ztserver provides user-space networking over ZeroTier to desktop applications, without requiring a commercial embedding license from ZeroTier, Inc. By launching ztserver as it's own process, your application avoids linking against the GPL licensed code of libzt.

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

Not yet implemented.

Will be used to bi-directionally forward UDP packets from the local machine to a ZeroTier address.

Effectively this will accept:
- A ZeroTier peer ID (or IP address) and a target port, and
- A local UDP port to forward incoming packets to

It will expose that remote client's port as a dynamic UDP port on the local machine. Sending packets to that port will be forwarded to the remote client. Packets received from the remote client will appear to originate from that dynamic UDP port and be sent to the local UDP port.

This method will return the dynamic UDP port so you can track the mapping of "remote client -> dynamic UDP port" in your application to resolve incoming packets back to their real identity.

**Request:** TBD

**Response:** Standard format (see above)

### PUT /unforward

Not yet implemented.

Will accept just a dynamic UDP port that was allocated from a previous forwarding and removes the forwarding.

**Request:** TBD

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

## License

`ztserver` is effectively GPL licensed because it links against `libzt` (which is GPL licensed), and statically links against `ulfius` which is LGPL licensed.

That said, the `main.cpp` file itself is provided under the MIT license, so if you want to use the code in that file independent of the dependencies, you are able to do so with the terms of the MIT license.