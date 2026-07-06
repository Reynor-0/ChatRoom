## ADDED Requirements

### Requirement: Login-state menu
The client SHALL display different menu options depending on whether the user is logged in.

#### Scenario: Not logged in
- **WHEN** the client starts and is not logged in
- **THEN** the menu shows: Register, Login, Exit

#### Scenario: Logged in
- **WHEN** the user has successfully logged in
- **THEN** the menu shows chat-related options (broadcast, private, online users) and Logout

### Requirement: Registration interaction
The client SHALL prompt for username and password, send a REGISTE command, and display the result.

#### Scenario: Registration success
- **WHEN** the user selects "Register" and enters a new username/password
- **THEN** the client displays "Registration success" and returns to the menu

#### Scenario: Registration failure
- **WHEN** the server returns NAME_EXIST
- **THEN** the client displays an error message and returns to the menu

### Requirement: Login interaction
The client SHALL prompt for username and password, send a LOGIN command, and transition to the logged-in state on success.

#### Scenario: Login success
- **WHEN** the server returns OP_OK for a LOGIN request
- **THEN** the client sets login state and shows the chat menu

### Requirement: Background receive thread
The client SHALL maintain a background thread that continuously receives server messages (login results, broadcasts, system notifications) and prints them.

#### Scenario: Server sends a message
- **WHEN** any server-side message is received
- **THEN** the client prints it to stdout
