# Networking and Concurrency

The server listens on a specified port for incoming client connections and handles the client's request based on custom protocol

Features:
- initializes server socket and binds it to specified port
- accepts incomming client connections
- creates new thread to handle each client request concurrently
- process client messages: LOGIN, ENROLLMENT, WAITLIST, CLIST (course list)
- manages user session, updates course enrollment and waitlist status, maintain server statistics appropriately
- implements signal handling which sends SIGINT (ctrl-c) signal to gracefully shut down server

Usage:
- to start server: <username> <localhost>
  - ./server <port_number> <course_filename> <log_filename>
  - ./bin/zotReg_server 3200 rsrc/course_1.txt log.txt
- to connect client:
  - ./client <username> <localhost> <port_number>
  - ./bin/zotReg_client twilly 127.0.0.1 3200
