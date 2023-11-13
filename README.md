# sv

A basic web server written in C with support for TLS

## Dependencies

This project depends on OpenSSL (`-lssl` and `-lcrypto`), libmagic and GCC.
If you are using a moderately recent linux distribution these should be already
present.

## How to build

`make`

## How to use

A default configuration is present in `config.conf`.
To invoke run `./sv config.conf` this will default to serving a test website on
port 9092

## Particularities

* This server is single threaded
I might make it multithreaded at some point I
tried to keep the number of "globals" to a minimum so it should be relatively
straight forwards.

* This project depends on GCC
In order to try and cut down on possible memory bugs, this project is build
with GCC's `-fanalyzer`.

* This server supports TLS

* This server currently only supports one connection per thread, this results
  in the server being pretty slow.

## What do do next

* Multi-threading

* More than 1 connection per thread, io\_uring or libuv both look interesting,
  probably will go with libuv.

* Make it so that the tls detection code does not break the handshake from the
  point of view of the ssl lib, `ungetc` perhaps.

* Lots of TODOs in the code.

* More tests
