## ADDED Requirements

### Requirement: Google-style function comments
Every function and method SHALL be preceded by a Google-style comment block that includes a brief description, parameter documentation, and return value documentation.

#### Scenario: Function with parameters and return value
- **WHEN** a function takes parameters and returns a value
- **THEN** the comment includes `@brief`, `@param` for each parameter, and `@return`

#### Scenario: Simple getter/setter
- **WHEN** a member function is a one-line getter or setter
- **THEN** a single-line `///` comment is sufficient

### Requirement: Class and struct documentation
Every class and struct SHALL have a brief description of its purpose and behavior.

#### Scenario: RAII class
- **WHEN** a class manages a resource
- **THEN** the comment explains what resource it owns, copy/move semantics, and lifecycle

### Requirement: Inline comments for key logic
Non-obvious code blocks SHALL have inline comments explaining intent.

#### Scenario: Socket accept loop
- **WHEN** code blocks involve system calls, thread creation, or protocol logic
- **THEN** a brief inline comment explains the purpose of that block

### Requirement: File header documentation
Every source file SHALL have a header comment describing the file's purpose, usage (compilation/runtime), and module role.

#### Scenario: Source file with main function
- **WHEN** a source file contains a `main` function
- **THEN** the file header includes build and run instructions
