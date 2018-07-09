CryptoKernel
============

CryptoKernel is a C++ library intended to help create blockchain-based digital currencies. It contains modules for key-value storage with JSON, peer-to-peer networking, ECDSA & Schnorr key generation, signing and verifying, big number operations, logging and a blockchain class for handling a Bitcoin-style write-only log. Designed to be object-oriented and easy to use, it provides transaction scripting with Lua 5.3, custom consensus algorithms (e.g Proof of Work, Authorised Verifier Round-Robin) and custom transaction types. 

Building on Ubuntu 16.04
------------------------

```
./installdeps.sh
premake5 gmake2
make
```

The resulting binary will be in the `bin/Static/Debug` directory. 

It is also possible to compile with other options. See `make help` and `premake5 --help`
for a list.

Usage
-----
See the source files in src/client/ for example usage. CryptoKernel ships with a Proof of Work coin built-in called K320 that works out of the box. For more information about the API check the documentation.

To launch CryptoKernel as K320 with its default setup, simply write:

```
./ckd
```

To get a list of command line RPC commands use:
```
./ckd help
```

To make ckd run in daemon mode use:
```
./ckd -daemon
```

API Reference
-------------

Build the documentation with doxyblocks.
```
premake5 gmake2 --with-docs
make
```

Roadmap
===

(in no particular order)

- Load external contract code to avoid duplication
when propagating contracts
- Standardised address format (not just public keys)
- Pay to MAST transaction type (includes pay to pubkey hash and pay to script hash)
- BFT consensus module
- Raft consensus module
- Proof of Stake consensus module
- HD key generation
- Schnorr signature aggregation for transactions
