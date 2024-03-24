# Networking and Concurrency

The server listens on a specified port for incoming client connections and handles the client's request based on custom protocol

Features:
- initializes server socket and binds it to specified port
- accepts incomming client connections
- creates new thread to handle each client request concurrently
- process client messages: LOGIN, ENROLLMENT, WAITLIST, CLIST (course list)
- manages user session, updates course enrollment and waitlist status, maintain server statistics appropriately
