CryptoKernel
============

CryptoKernel is a C++ library intended to help create blockchain-based digital currencies. It contains modules for key-value storage with JSON, peer-to-peer networking, ECDSA & Schnorr key generation, signing and verifying, big number operations, logging and a blockchain class for handling a Bitcoin-style write-only log. Designed to be object-oriented and easy to use, it provides transaction scripting with Lua 5.3, custom consensus algorithms (e.g Proof of Work, Authorised Verifier Round-Robin) and custom transaction types. 

K320
------------------------
We have used Cryptokernel to implement an experimental digital currency called K320. Its name derives from its monetary policy – Milton Friedman’s K% rule at a rate of 320 basis points or 3.2% growth per year (https://en.wikipedia.org/wiki/Friedman%27s_k-percent_rule). This known rate removes the need for trust in the decisions of a central authority, making monetary supply policy completely transparent. K320 is designed to build up the initial money supply relatively quickly then switch to the K% rule for monetary growth. 

In K320, blocks are produced every 2.5 minutes. The block reward starts at 100 coins per block, diminishing for 8 years until the year 2025 at block 1,741,620 and a supply of 68,720,300 coins. At this point the block reward switches to a constant 3.2% per year. At 210,240 blocks per year, that is .0000152% of total supply per block as reward. For example, the first block reward after the switch to the constant reward rate will be ~10.45; (0.000000152*68,720,300). For a more detailed description of this see: https://github.com/mit-dci/CryptoKernel/blob/master/src/client/multicoin.cpp#L80

K320 is currently implemented by default when you run the CryptoKernel software.

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
- BFT consensus module
- Raft consensus module
- Proof of Stake consensus module
- HD key generation
