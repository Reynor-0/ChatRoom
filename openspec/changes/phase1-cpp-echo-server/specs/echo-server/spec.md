## ADDED Requirements

### Requirement: Server listens on a port
The server SHALL accept a port number as a command-line argument and listen for incoming TCP connections on that port.

#### Scenario: Server starts successfully
- **WHEN** the server is started with a valid port number (e.g., `./server 8888`)
- **THEN** the server binds to the port and blocks waiting for client connections

#### Scenario: Invalid port argument
- **WHEN** the server is started without arguments or with an invalid port
- **THEN** the server prints a usage message to stderr and exits with code 1

### Requirement: Server accepts multiple clients
The server SHALL accept multiple simultaneous TCP client connections, each handled in its own thread.

#### Scenario: Two clients connect
- **WHEN** two clients connect to the server
- **THEN** each client's messages are independently echoed back to it

### Requirement: Server echoes received messages
The server SHALL receive text from a client and send the same text back to that same client.

#### Scenario: Client sends a message
- **WHEN** a client sends "hello world"
- **THEN** the server receives it, prints it to stdout, and sends "hello world" back to the same client

### Requirement: Server handles client disconnect
The server SHALL detect client disconnection and clean up resources without crashing.

#### Scenario: Client disconnects
- **WHEN** a client closes its connection
- **THEN** the server closes the corresponding socket and terminates that client's handler thread
