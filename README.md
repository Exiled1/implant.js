# implant.js

A **proof-of-concept**\* modular implant platform leveraging the [v8 JavaScript engine](https://v8.dev).

Initially released and presented at DistrictCon 0. Slides and recording will be added here after publication.

_* there is no authentication, encryption, or module verification being performed, and zero regards for on-target OPSEC. if you use this on an op, you will look dumb and if i find out i will shame you on social media :)_

## Usage

Please review the [client](/client/README.md) and [server](/server/README.md) docs for detailed compilation/usage info, but the tl;dr is:

```
$ cd server
$ ./server.py
```

```
$ cd client
$ mkdir build
$ cmake -S . -B build -DV8_ROOT:STRING=/path/to/v8/root -DBUILD_DEBUG=True
$ cmake --build build
$ ./build/client localhost 1337
```

## Testing

The client and server have their own test suite, and there is an end-to-end integration test script available [here](/tests/e2e.py) to validate the comms between the client and the server.

## Detections

For info about detecting various facets of implant.js, please see [`/detections`](/detections).

## Known Issues

There are a couple pieces with minor bugs that haven't been addressed yet, please see the repo issues for details. PRs are welcome :)

## Misc

To cleanup a hanging server port allocation (doesn't always work for some reason):
```
$ fuser -k 1337/tcp
```

To watch the C2 traffic live:
```
$ tshark -i lo -f 'port 1337' -Y "tcp.payload" -T fields -e tcp.dstport -e tcp.payload
```