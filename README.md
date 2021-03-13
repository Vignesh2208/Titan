# Titan
Titan: A high precision virtual time system based on binary instrumentation.

Full documentation can be found [here](https://titan-vt.readthedocs.io/en/latest/index.html)


Note
====

This is a prototype implementation with the primary goal of encouraging more research in the field of virtual time driven network emulation and simulation. It does not support all general requirements yet and may contain bugs. Contributions to fix any identified issues are welcome ! Please contact me at vig2208@gmail.com.

TODO
====

* A slight modification to the lookahead algorithm might be required to improve lookahead estimates in the event of packet drops in the simulated network. Currently it appears lookahead estimates are small if an in-transit packet gets dropped. This needs to be investigated.
* The virtual socket layer does not support poll and select interface yet. This feature will be added soon.
