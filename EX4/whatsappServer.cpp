#include "whatsappServer.h"
#include "whatsappio.h"

struct server_struct
{
    int socket;
    std::vector<std::string> ordered_clients;
    std::unordered_map<std::string, int> connected_clients;
    std::unordered_map<std::string, std::vector<std::string>> groups;
    fd_set fds;
}server;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        print_server_usage();
        exit(1);
    }
    int port = char_to_int(argv[1]);
    setup(port);
    server_protocol();
    return 0;
}

void setup(int port)
{
    char host_name[MAX_HOST_LEN];
    gethostname(host_name, MAX_HOST_LEN);
    server.socket = establish_connection(host_name, port, true);
}

void server_protocol()
{
    while(true)
    {
        build_fd_set(&server.fds);
        int max_fd = get_max_fd();
        if((select(max_fd + 1, &server.fds, NULL, NULL, NULL)) < 0)
        {
            print_error(SELECT, errno);
            exit(1);
        }
        if (FD_ISSET(STDIN_FILENO, &server.fds))
        {
            handle_std_in();
        }
        if (FD_ISSET(server.socket, &server.fds))
        {
            connect_new_client();
        }
        else
        {
            for(auto it = server.connected_clients.begin(); it != server.connected_clients.end(); ++it)
            {
                if (FD_ISSET(it->second, &server.fds))
                {
                    handle_client_request(it->second, it->first);
                    break;
                }
            }
        }
    }
}

void handle_std_in()
{
    std::string command;
    std::getline(std::cin, command);
    if(command.compare("EXIT") == 0)
    {
        std::string exit = SERVER_EXIT;
        for(auto it = server.connected_clients.begin(); it != server.connected_clients.end(); ++it)
        {
            if(write_output(it->second, exit) < 0)
            {
                print_error(WRITE, errno);
                std::exit(1);
            }
        }
        print_exit();
        std::exit(0);
    }
    print_invalid_input();
}

void build_fd_set(fd_set *fds)
{
    FD_ZERO(fds);
    FD_SET(STDIN_FILENO, fds);
    FD_SET(server.socket, fds);
    for(auto it = server.connected_clients.begin(); it != server.connected_clients.end(); ++it)
    {
        FD_SET(it->second, fds);
    }
}

void connect_new_client()
{
    int connect_me;
    if ((connect_me = accept(server.socket, NULL, NULL)) < 0)
    {
        print_error(ACCEPT, errno);
        exit(1);
    }
    std::string new_client;
    if (read_input(connect_me, new_client) < 0)
    {
        print_error(READ, errno);
        exit(1);
    }
    char bit;
    if(exists(new_client, false) == 0)
    {
        bit = '0';
        print_fail_connection();
    }
    else
    {
        bit = '1';
        print_connection_server(new_client);
    }
    if (indicate_peer(connect_me, bit) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    std::pair<std::string, int> newPair(new_client, connect_me);
    server.connected_clients.insert(newPair);
    server.ordered_clients.push_back(new_client);
    sort(server.ordered_clients.begin(), server.ordered_clients.end());
}

void handle_client_request(int fd, const std::string& caller)
{
    std::string raw_command;
    if(read_input(fd, raw_command) < 0)
    {
        print_error(READ, errno);
        exit(1);
    }
    command_type ct;
    std::string message, name;
    std::vector<std::string> members;
    parse_command(raw_command, ct, name, message, members);
    char bit;
    switch(ct)
    {
        case CREATE_GROUP:
            if (create_group(name, caller, members) < 0)
            {
                bit = '0';
                print_create_group(true, false, caller, name);
            }
            else
            {
                bit = '1';
                print_create_group(true, true, caller, name);
            }
            if (indicate_peer(fd, bit) < 0)
            {
                print_error(WRITE, errno);
                exit(1);
            }
            break;
        case SEND:
            if(send_msg(name, caller, message) < 0)
            {
                bit = '0';
                print_send(true, false, caller, name, message);
            }
            else
            {
                bit = '1';
                print_send(true, true, caller, name, message);
            }
            if (indicate_peer(fd, bit) < 0)
            {
                print_error(WRITE, errno);
                exit(1);
            }
            break;
        case WHO:
            print_who_server(caller);
            who(fd);
            break;
        case EXIT:
            exit_client(fd, caller);
            print_exit(true, caller);
            break;
        case INVALID:
            print_invalid_input();
            break;
    }
}

void exit_client(int fd, const std::string& caller)
{
    close(fd);
    for(auto it = server.groups.begin(); it != server.groups.end(); ++it)
    {
        for(unsigned int i = 0; i < it->second.size(); i++)
        {
            if(it->second[i].compare(caller) == 0)
            {
                it->second.erase(it->second.begin() + i);
            }
        }
    }
    auto it = server.groups.begin();
    while (it != server.groups.end())
    {
        if (it->second.empty())
        {
            it = server.groups.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for(auto it = server.connected_clients.begin(); it != server.connected_clients.end(); ++it)
    {
        if (caller.compare(it->first) == 0)
        {
            server.connected_clients.erase(it);
            break;
        }
    }
    for(unsigned int i = 0; i < server.ordered_clients.size(); ++i)
    {
        if(caller.compare(server.ordered_clients[i]) == 0)
        {
            server.ordered_clients.erase(server.ordered_clients.begin() + i);
            break;
        }
    }
    server.connected_clients.erase(caller);
}

int who(int fd)
{
    std::string who_list;
    for (unsigned int i = 0; i < server.ordered_clients.size(); i++)
    {
        who_list += server.ordered_clients[i];
        if (i != server.ordered_clients.size() - 1)
        {
            who_list += ",";
        }
    }
    if(write_output(fd, who_list) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    if (read_bit(fd) == '0')
    {
        return -1;
    }
    return 0;
}

int send_msg(std::string& send_to, const std::string& sender, std::string& message)
{
    std::string msg_to_client(sender + ": " + message);
    auto found1 = server.groups.find(send_to);
    if (found1 != server.groups.end())
    {
        return send_to_group(sender, msg_to_client, found1->second);
    }
    auto found2 = server.connected_clients.find(send_to);
    if (found2 != server.connected_clients.end())
    {
        return send_to_client(found2->second, msg_to_client);
    }
    return -1;
}

int send_to_client(int fd, std::string& message)
{
    if(write_output(fd, message) < 0)
    {
        print_error(WRITE, errno);
        exit(1);
    }
    if (read_bit(fd) == '0')
    {
        return -1;
    }
    return 0;
}

int send_to_group(const std::string& sender, std::string& message, std::vector<std::string>& group)
{
    bool sender_in_group = false;
    for(std::string& client : group)
    {
        if(client.compare(sender) == 0)
        {
            sender_in_group = true;
        }
    }
    if(!sender_in_group)
    {
        return -1;
    }
    for(std::string& client : group)
    {
        if(client.compare(sender) == 0)
        {
            continue;
        }
        int fd = server.connected_clients[client];
        if(write_output(fd, message) < 0)
        {
            print_error(WRITE, errno);
            exit(1);
        }
        if (read_bit(fd) == '0')
        {
            return -1;
        }
    }
    return 0;
}


int create_group(std::string& group_name, const std::string& caller, std::vector<std::string>& group_members)
{
    group_members.push_back(caller);
    remove_duplicates(group_members);
    for(std::string& member : group_members)
    {
        if(exists(member, true) < 0)
        {
            return -1;
        }
    }
    if(exists(group_name, false) == 0)
    {
        return -1;
    }
    std::vector<std::string> new_group(group_members);
    std::pair<std::string, std::vector<std::string>> pair(group_name, new_group);
    server.groups.insert(pair);
    return 0;
}

int exists(std::string& search_me, bool client)
{
    auto found = server.connected_clients.find(search_me);
    if (found != server.connected_clients.end())
    {
        return 0;
    }
    if (!client)
    {
        auto found = server.groups.find(search_me);
        if (found != server.groups.end())
        {
            return 0;
        }
    }
    return -1;
}

void remove_duplicates(std::vector<std::string>& names)
{
    sort(names.begin(), names.end());
    std::vector<std::string>::iterator iter = names.begin();
    while(iter != names.end())
    {
        std::string prev = *(iter);
        iter++;
        if (iter == names.end())
        {
            break;
        }
        while (iter != names.end() && prev.compare(*(iter)) == 0)
        {
            iter = names.erase(iter);
        }
    }
}

int get_max_fd()
{
    int max = server.socket;
    for(auto it = server.connected_clients.begin(); it != server.connected_clients.end(); ++it)
    {
        if(it->second > max)
        {
            max = it->second;
        }
    }
    return max;
}


