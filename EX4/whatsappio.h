#ifndef _WHATSAPPIO_H
#define _WHATSAPPIO_H

#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
#include <zconf.h>
#include <cmath>
#include <netdb.h>
#include <cerrno>
#include <arpa/inet.h>

#define WA_MAX_NAME 30
#define WA_MAX_MESSAGE 256
#define WA_MAX_GROUP 50
#define WA_MAX_INPUT ((WA_MAX_NAME+1)*(WA_MAX_GROUP+2))
#define WA_MAX_PEND 10
#define SERVER_EXIT "serverexit"
#define GET_HOST_BY_NAME "gethostbyname"
#define SOCKET "socket"
#define BIND "bind"
#define LISTEN "listen"
#define SELECT "select"
#define ACCEPT "accept"
#define CONNECT "connect"
#define READ "read"
#define WRITE "write"


enum command_type {CREATE_GROUP, SEND, WHO, EXIT, INVALID};

/**
 * Creates a socket in the given address and port
 * @param addr
 * @param port
 * @param server true if the socket is created for a server
 * @return file descriptor of the socket
 */
int establish_connection(char* addr, int port, bool server);
/**
 * Write given message into the socket fd
 * @param fd
 * @param message
 * @return Negative value upon failure
 */
int write_output(int fd, std::string& message);
/**
 * Reads a message from the socket fd
 * @param fd
 * @param fill_me this will contain the read message
 * @return Negative value upon failure
 */
int read_input(int fd, std::string& fill_me);
/**
 * Reads one char from the socket fd
 * @param fd
 * @return read char
 */
char read_bit(int fd);
/**
 * Sends one char to the socket fd
 * @param fd
 * @param ind char to send
 * @return Negative value upon failure
 */
int indicate_peer(int fd, char& ind);
/**
 * Converts the decimal number described by convert_me into int
 * @param convert_me
 * @return converted int
 */
int char_to_int(char* convert_me);

/*
 * Description: Prints to the screen a message when the user terminate the
 * server
*/
void print_exit();

/*
 * Description: Prints to the screen a message when the client established
 * connection to the server, in the client
*/
void print_connection();

/*
 * Description: Prints to the screen a message when the client established
 * connection to the server, in the server
 * client: Name of the sender
*/
void print_connection_server(const std::string& client);

/*
 * Description: Prints to the screen a message when the client tries to
 * use a name which is already in use
*/
void print_dup_connection();

/*
 * Description: Prints to the screen a message when the client fails to
 * establish connection to the server
*/
void print_fail_connection();

/*
 * Description: Prints to the screen the usage message of the server
*/
void print_server_usage();

/*
 * Description: Prints to the screen the usage message of the client
*/
void print_client_usage();

/*
 * Description: Prints to the screen the messages of "create_group" command
 * server: true for server, false for client
 * success: Whether the operation was successful
 * client: Client name
 * group: Group name
*/
void print_create_group(bool server, bool success, const std::string& client, const std::string& group);

/*
 * Description: Prints to the screen the messages of "send" command
 * server: true for server, false for client
 * success: Whether the operation was successful
 * client: Client name
 * name: Name of the client/group destination of the message
 * message: The message
*/
void print_send(bool server, bool success, const std::string& client, const std::string& name, const std::string& message);

/*
 * Description: Prints to the screen the messages recieved by the client
 * client: Name of the sender
 * message: The message
*/
void print_message(const std::string& client, const std::string& message);

/*
 * Description: Prints to the screen the messages of "who" command in the server
 * client: Name of the sender
*/
void print_who_server(const std::string& client);

/*
 * Description: Prints to the screen the messages of "who" command in the client
 * success: Whether the operation was successful
 * clients: a vector containing the names of all clients
*/
void print_who_client(bool success, const std::vector<std::string>& clients);

/*
 * Description: Prints to the screen the messages of "exit" command
 * server: true for server, false for client
 * client: Client name
*/
void print_exit(bool server, const std::string& client);

/*
 * Description: Prints to the screen the messages of invalid command
*/
void print_invalid_input();

/*
 * Description: Prints to the screen the messages of system-call error
*/
void print_error(const std::string& function_name, int error_number);

/*
 * Description: Parse user input from the argument "command". The other arguments
 * are used as output of this function.
 * command: The user input
 * commandT: The command type
 * name: Name of the client/group
 * message: The message
 * clients: a vector containing the names of all clients
*/
void parse_command(const std::string& command, command_type& commandT, std::string& name, std::string& messsage, std::vector<std::string>& clients);

#endif
