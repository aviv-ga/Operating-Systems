#ifndef EX4_WHATSAPPCLIENT_H
#define EX4_WHATSAPPCLIENT_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <netdb.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <vector>

/**
 * Runs the whatsapp client logic
 * @param client_name Name of the client
 * @param server_addr Server IP
 * @param server_port_string Port
 * @return Negative value upon failure
 */
int run_client(char* client_name, char* server_addr, char* server_port_string);
/**
 * Sets up clients connection with server
 * @param client_name Name of the client
 * @param server_addr Server IP
 * @param port Port
 */
void connection_setup(std::string& client_name, char* server_addr, int port);
/**
 * Runs the protocol loop of the client
 */
void protocol();
/**
 * Checks that the given sting contains only alphabetic characters and numbers
 * @param client_name
 * @return 0 if the conditions hold, -1 otherwise
 */
int check_name(std::string& client_name);
/**
 * Checks if the given name is in use in the server
 * @param client_name
 * @return 0 if client_name is a valid client valid name in the server, -1 otherwise
 */
int check_existing_name(std::string& client_name);
/**
 * Handles user input
 */
void handle_stdin();
/**
 * Handles server input
 */
void handle_server_input();
/**
 * Builds new fd_sets for protocol iteration
 * @param set
 */
void build_fd_set(fd_set* set);
/**
 * Handles create_group command input by the user
 * @param group_name
 * @param group_members
 * @param command
 * @return Negative value upon failure
 */
int create_group(std::string& group_name, std::vector<std::string>& group_members , std::string& command);
/**
 * Handles send command input by the user
 * @param send_to
 * @param message
 * @param command
 * @return Negative value upon failure
 */
int send_msg(std::string& send_to, std::string& command);
/**
 * Handles who command input by the user
 * @param fill_me Vector that will be filled with the clients in the server
 * @param command
 * @return Negative value upon failure
 */
int who(std::vector<std::string>& fill_me, std::string& command);
/**
 * Handles exit command input by the user
 * @param command
 * @return Negative value upon failure
 */
int perform_exit(std::string& command);
/**
 * Fills the fill_me with strings that are separated by a comma in command
 * @param fill_me
 * @param command
 */
void fill_vec_with_names(std::vector<std::string>& fill_me, std::string& command);

#endif //EX4_WHATSAPPCLIENT_H
