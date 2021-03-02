#ifndef EX4_WHATSAPPSERVER_H
#define EX4_WHATSAPPSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <netdb.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <arpa/inet.h>
#include <algorithm>

#define MAX_HOST_LEN 100

/**
 * Sets up the server connection
 * @param port
 */
void setup(int port);
/**
 * Runs the protocol loop of the client
 */
void server_protocol();
/**
 * Builds new fd_sets for protocol iteration
 * @param set
 */
void build_fd_set(fd_set* set);
/**
 * Handles a connection attempt from a new client
 */
void connect_new_client();
/**
 * Handles a request from a client
 * @param fd file descriptor of the client
 * @param caller name of the client
 */
void handle_client_request(int fd, const std::string& caller);
/**
 * Checks whether search_me in use in the server
 * @param search_me
 * @param client indicates on whether we want to check also amongs group names
 * @return true if condition holds, false otherwise
 */
int exists(std::string& search_me, bool client);
/**
 * Handles user input
 */
void handle_std_in();
/**
 * Handles create_group command input by a client
 * @param group_name
 * @param caller
 * @param group_members
 * @return Negative value upon failure
 */
int create_group(std::string& group_name, const std::string& caller, std::vector<std::string>& group_members);
/**
 * Handles send command input by a client
 * @param send_to
 * @param sender
 * @param message
 * @return Negative value upon failure
 */
int send_msg(std::string& send_to, const std::string& sender, std::string& message);
/**
 * Sends message to the participants of group, excluding sender
 * @param sender
 * @param message
 * @param group
 * @return Negative value upon failure
 */
int send_to_group(const std::string& sender, std::string& message, std::vector<std::string>& group);
/**
 * Sends message to the client associated with the file descriptor fd
 * @param fd file descriptor
 * @param sender
 * @param message
 * @return Negative value upon failure
 */
int send_to_client(int fd, std::string& message);
/**
 * Handles who command input by a client
 * @param fd file descriptor of the client
 * @return Negative value upon failure
 */
int who(int fd);
/**
 * Handles exit command input by a client
 * @param fd file descriptor of the client
 * @param caller
 */
void exit_client(int fd, const std::string& caller);
/**
 * Returns the file descriptor in use with the maximal value
 */
int get_max_fd();
/**
 * Removes duplicates from the vector names
 * @param names
 */
void remove_duplicates(std::vector<std::string>& names);

#endif //EX4_WHATSAPPSERVER_H
