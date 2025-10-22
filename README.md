# flare.c

Unusable in production. Just wanted to write some C code.
For reliable reverse proxy use the original [Flare](https://github.com/gogi1337/flare)

## How to run?
- `make`
- Setup config.ini (also remove the .example suffix)
- `sudo setcap 'CAP_NET_BIND_SERVICE+ep' ./flare`
- `./flare`