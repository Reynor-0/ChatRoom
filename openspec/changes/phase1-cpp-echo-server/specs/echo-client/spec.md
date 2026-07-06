## ADDED Requirements

### Requirement: Client connects to server
The client SHALL accept a server IP address and port number as command-line arguments, and establish a TCP connection to the server.

#### Scenario: Successful connection
- **WHEN** the client is started with valid IP and port (e.g., `./client 127.0.0.1 8888`)
- **THEN** the client connects to the server and is ready to send messages

#### Scenario: Invalid arguments
- **WHEN** the client is started with wrong number of arguments or invalid port
- **THEN** the client prints a usage message to stderr and exits

### Requirement: Client sends user input
The client SHALL read lines from standard input and send them to the server.

#### Scenario: User types a message
- **WHEN** the user types "hello world" and presses enter
- **THEN** the client sends "hello world" to the server

#### Scenario: Message with spaces
- **WHEN** the user types a message containing spaces (e.g., "hi there friend")
- **THEN** the entire message including spaces is sent to the server

### Requirement: Client receives and displays echoed messages
The client SHALL receive messages from the server and print them to standard output.

#### Scenario: Echo received
- **WHEN** the server echoes back a message
- **THEN** the client prints the received message to stdout
