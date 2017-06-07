CryptoKernel
============

CryptoKernel is a C++ library intended to help create blockchain-based digital currencies. It contains modules for key-value storage with JSON, peer-to-peer networking, ECDSA key generation, signing and verifying, big number operations, logging and a blockchain class for handling a Bitcoin-style write-only log. Designed to be object-oriented and easy to use, it provides transaction scripting with Lua 5.3, custom consensus algorithms (e.g Raft, Proof of Work, Authorised Verifier Round-Robin) and custom transaction types. 

Building on Ubuntu 16.04
------------------------

```
./installdeps.sh
make
```

Usage
-----
See the source files in src/client/ for example usage. CryptoKernel ships with a Proof of Work consensus method built in that works out of the box. For more information about the API check the documentation.

To launch CryptoKernel as a currency with its default setup, simply write:

```
./ckd
```

To get a list of command line RPC commands use:
```
./ckd help
```

There is also a GUI for CryptoKernel that runs client-side in a web browser available here: https://github.com/metalicjames/ckui

API Reference
-------------

Build the documentation with doxyblocks.
```
doxygen .
```
