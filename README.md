# Diameter TCP to SCTP Proxy

## How it works ?

* Listens for incoming TCP connections
* Creates a SCTP connection
* Buffers diameter messages to fit a single SCTP message
* 1 tcp to 1 sctp connection

## TODO

* List of SCTP endpoints for load balancing
* Multiple SCTP associations (master/slave)

