# libcast

libcast is an open source implementation of the Cast procotol supporting Cast
applications and streaming to Cast-compatible devices.

## Using the standalone implementations

To run the standalone sender and receivers together, first you need to install
the following dependencies: FFMPEG, LibVPX, LibOpus, LibSDL2, as well as their
headers (frequently in a seperate -dev package). From here, you just need a
video to use with the cast_sender, as the cast_receiver can generate a
self-signed certificate and private key for each session. You can also generate
your own RSA private key and either create or have the receiver automatically
create a self signed certificate with that key. If the receiver generates a root
certificate, it will print out the location of that certificate to stdout.

```
  $ /path/to/out/Default/cast_receiver <interface>
  // Also fine:
  $ /path/to/out/Default/cast_receiver <interface> -p <private_key>
  $ /path/to/out/Default/cast_receiver <interface> -p <private_key> -s <certificate>

  $ /path/to/out/Default/cast_sender -s <certificate> <path/to/video>
```


From there, after building Open Screen the `cast_sender` and `cast_receiver`
executables should be ready to use:

When running on Mac OS X, also pass the `-x` flag to the cast receiver to
disable DNS-SD/mDNS, since Open Screen does not currently integrate with
Bonjour.

When connecting to a receiver that's not running on the loopback interface
(typically `lo` or `lo0`), pass the `-r <receiver IP endpoint>` flag to the
`cast_sender` binary.
