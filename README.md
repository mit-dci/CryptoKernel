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

Roadmap
===

## Legend

   - H -- Hard Fork
   - S -- Soft Fork
   - L -- Local (No Fork Required)

## Versions
#### alpha v0.0.1 -- H
- H -- Transaction Merkle root in block header
- H -- Rule change signalling in block
- L -- Wallet tracks unconfirmed transaction
- L -- Wallet schema version detection
- H -- Friedman rule emission schedule
- H -- (Potentially) BRNDF instead of KGW for difficulty
- H -- Lyra2REv2 as PoW function 
    
#### alpha v0.1.0 -- S 
- S -- Expand Lua contract standard library with more
digital currency specific functions
- S -- Load external contract code to avoid duplication
for propagating contracts
- S -- Schnorr signatures
- L -- Seed addresses
- L -- Standardised address format (not just public keys)

#### alpha v0.2.0
- L -- HD key generation and recovery with seed
- L -- LN integrated with wallet
