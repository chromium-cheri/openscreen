# Cast Streaming

This module contains an implementation of Cast Streaming, the real-time media
streaming protocol between Cast senders and Cast receivers.

Included are two applications, `cast_sender` and `cast_receiver` that
demonstrate how to send and receive media using a Cast Streaming session.

## Prerequisites

To run the Cast Streaming sender and receiver, you need to install the following
dependencies: FFMPEG, LibVPX, LibOpus, LibSDL2, LibAOM as well as their headers
(frequently in a separate -dev package). Currently, it is advised that most
Linux users compile LibAOM from source, using the instructions at
https://aomedia.googlesource.com/aom/. Older versions found in many package
management systems have blocking performance issues, causing AV1 encoding to be
completely unusable.

## Compilation

```bash
gn gen out/debug
autoninja -C out/debug cast_sender cast_receiver
```

## Developer certificate generation and use

To use the sender and receiver application together, a valid Cast certificate is
required. The easiest way to generate the certificate is to just run the
`cast_receiver` with `-g`, and both should be written out to files:

```
  $ out/debug/cast_receiver -g
    [INFO:../../cast/receiver/channel/static_credentials.cc(161):T0] Generated new private key for session: ./generated_root_cast_receiver.key
    [INFO:../../cast/receiver/channel/static_credentials.cc(169):T0] Generated new root certificate for session: ./generated_root_cast_receiver.crt
```

## Running

In addition to the certificate, to start a session you need a valid network
address and path to a video for the sender to play.

```bash
./out/debug/cast_receiver -d generated_root_cast_receiver.crt -p generated_root_cast_receiver.key lo0

./out/debug/cast_sender -d generated_root_cast_receiver.crt -r 127.0.0.1 ~/video-1080-mp4.mp4
```

TODO(https://issuetracker.google.com/197659238): Fix discovery mode for `cast_sender`. 

### Mac

When running on Mac OS X, also pass the `-x` flag to `cast_receiver` to disable
DNS-SD/mDNS, since Open Screen does not currently integrate with Bonjour.

# End-to-end tests

The script [`standalone_e2e.py`](../standalone_e2e.py) exercises the Cast Sender
sender and receiver through an end-to-end test.
