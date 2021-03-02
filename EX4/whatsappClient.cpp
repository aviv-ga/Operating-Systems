#include "whatsappClient.h"
#include "whatsappio.h"

struct client_struct
{
    std::string name;
    int socket;
    fd_set fds;
} client;


int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        print_client_usage();
        exit(1);
    }
    if (run_client(argv[1], argv[2], argv[3]) < 0)
    {
        print_client_usage();
        exit(1);
    }
    return 0;
}

int run_client(char* client_name, char* server_addr, char* server_port_string)
{
    int server_port = char_to_int(server_port_string);
    std::string name(client_name);
    if(check_name(name) < 0)
    {
        return  -1;
    }
    connection_setup(name, server_addr, server_port);
    protocol();
    return 0;
}

void connection_setup(std::string& client_name, char* server_addr, int server_port)
{
    client.socket = establish_connection(server_addr, server_port, false);
    if (check_existing_name(client_name) < 0)
    {
        print_dup_connection();
        exit(1);
    }
    client.name = client_name;
    print_connection();
}

void protocol()
{
    while(true)
    {
        build_fd_set(&client.fds);
        if (select(client.socket + 1, &client.fds, NULL, NULL, NULL) < 0)
        {
            print_error(SELECT, errno);
            exit(1);
        }
        if (FD_ISSET(STDIN_FILENO, &client.fds))
        {
            handle_stdin();
        }
        else
        {
            handle_server_input();
        }
    }
}

void handle_stdin()
{
    std::string command;
    std::getline(std::cin, command);
    command_type ct;
    std::string name;
    std::string message;
    std::vector<std::string> members;
    parse_command(command, ct, name, message, members);
    if(command.empty() || command.find_first_not_of(' ') == std::string::npos)
    {
        print_invalid_input();
        return;
    }
    if(!name.empty() && check_name(name) < 0)
    {
        print_invalid_input();
        return;
    }
    switch(ct)
    {
        case CREATE_GROUP:
            (create_group(name, members, command) < 0) ? (print_create_group(false, false, client.name, name))
                                                       : (print_create_group(false, true, client.name, name));
            break;
        case SEND:
            (send_msg(name, command) < 0) ? (print_send(false, false, client.name, name, message))
                                          : (print_send(false, true, client.name, name, message));
            break;
        case WHO:
            (who(members, command) < 0) ? (print_who_client(false, members)) : (print_who_client(true, members));
            break;
        case EXIT:
            perform_exit(command);
            print_exit(false, client.name);
            exit(0);
        case INVALID:
            print_invalid_input();
            break;
    }
}

void handle_server_input()
{
    std::string server_input;
    if (read_input(client.socket, server_input) < 0)
    {
        print_error(READ, errno);
        exit(1);
    }
    if(server_input.compare(SERVER_EXIT) == 0)
    {
        close(client.socket);
        exit(0);
    }
    char bit = '1';
    indicate_peer(client.socket, bit);
    printf("%s\n", server_input.c_str());
}

int perform_exit(std::string& command)
{
    if (write_output(client.socket, command) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    close(client.socket);
    return 0;
}

int who(std::vector<std::string>& fill_me, std::string& command)
{
    if(write_output(client.socket, command) < 0)
    {
        return -1;
    }
    std::string client_names;
    if(read_input(client.socket, client_names) < 0)
    {
        print_error(READ, errno);
        exit(1);
    }
    fill_vec_with_names(fill_me, client_names);
    char bit = '1';
    if (indicate_peer(client.socket, bit) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    };
    return 0;
}

int send_msg(std::string& send_to, std::string& command)
{
    if(send_to.compare(client.name) == 0)
    {
        return -1;
    }
    if(write_output(client.socket, command) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    if(read_bit(client.socket) == '0')
    {
        return -1;
    }
    return 0;
}


int create_group(std::string& group_name, std::vector<std::string>& group_members, std::string& command)
{
    if(group_name.size() < 1 || group_members.size() < 1)
    {
        return -1;
    }
    if (client.name.compare(group_name) == 0)
    {
        return -1;
    }
    if(group_members.size() == 1)
    {
        if (client.name.compare(group_members[0]) == 0)
        {
            return -1;
        }
    }
    for(std::string member : group_members)
    {
        if(check_name(member) < 0 || (group_name.compare(member) == 0))
        {
            return -1;
        }
    }
    if(write_output(client.socket, command) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    if(read_bit(client.socket) == '0')
    {
        return -1;
    }
    return 0;
}

void build_fd_set(fd_set* set)
{
    FD_ZERO(set);
    FD_SET(STDIN_FILENO, set);
    FD_SET(client.socket, set);
}

int check_name(std::string& client_name)
{
    int len = client_name.size();
    if(len == 0 || len > WA_MAX_NAME)
    {
        return -1;
    }
    for(char c : client_name)
    {
        if(!(isdigit(c) || isalpha(c)))
        {
            return -1;
        }
    }
    return 0;
}

int check_existing_name(std::string& client_name)
{
    if (write_output(client.socket, client_name) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    if(read_bit(client.socket) == '0')
    {
        return -1;
    }
    return 0;
}


void fill_vec_with_names(std::vector<std::string>& fill_me, std::string& concat_names)
{
    std::string new_name;
    for(unsigned int i = 0; i < concat_names.size(); i++)
    {
        if(i == (concat_names.size() - 1))
        {
            new_name += concat_names[i];
            fill_me.push_back(new_name);
            break;
        }
        if(concat_names[i] != ',')
        {
            new_name += concat_names[i];
        }
        else
        {
            fill_me.push_back(new_name);
            new_name.clear();
        }
    }
}


