## ADDED Requirements

### Requirement: User registration
The server SHALL accept registration requests and store username/password pairs. Duplicate usernames SHALL be rejected.

#### Scenario: Successful registration
- **WHEN** a client sends a REGISTE command with a new username and password
- **THEN** the server stores the credentials and returns OP_OK

#### Scenario: Duplicate username
- **WHEN** a client sends a REGISTE command with an already-registered username
- **THEN** the server returns NAME_EXIST and does not overwrite the existing entry

### Requirement: User login
The server SHALL authenticate users by verifying username and password against stored credentials.

#### Scenario: Successful login
- **WHEN** a client sends a LOGIN command with correct username and password
- **THEN** the server marks the user as online (records socket fd) and returns OP_OK

#### Scenario: Wrong password
- **WHEN** a client sends a LOGIN command with an existing username but wrong password
- **THEN** the server returns NAME_PWD_NMATCH

#### Scenario: Already online
- **WHEN** a client attempts to login with credentials of an already-online user
- **THEN** the server returns USER_LOGED

### Requirement: Online user tracking
The server SHALL maintain an array of user records tracking registration status, online status, and associated socket fd.

#### Scenario: User goes offline
- **WHEN** a logged-in client disconnects
- **THEN** the server resets the user's fd to -1 (offline) but preserves registration data

### Requirement: Command dispatch
The server SHALL interpret incoming messages as Protocol structs and dispatch to the appropriate handler based on the cmd field.

#### Scenario: Unknown command
- **WHEN** the server receives a message with an unrecognized cmd value
- **THEN** the server ignores it (no crash, no response)
