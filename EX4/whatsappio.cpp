#include "whatsappio.h"

int establish_connection(char* addr, int port, bool server)
{
    int sock;
    hostent* hp = gethostbyname(addr);
    struct sockaddr_in sa;
    if (hp == NULL)
    {
        print_error(GET_HOST_BY_NAME, errno);
        exit(1);
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = hp->h_addrtype;
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_port = htons((u_short)port);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        print_error(SOCKET, errno);
        exit(1);
    }
    if(server)
    {
        if (bind(sock, (struct sockaddr *)&sa , sizeof(sa)) < 0)
        {
            print_error(BIND, errno);
            exit(1);
        }
        if(listen(sock, WA_MAX_PEND) < 0)
        {
            print_error(LISTEN, errno);
            exit(1);
        }
    }
    else
    {
        if (connect(sock, (struct sockaddr *)&sa , sizeof(sa)) < 0)
        {
            print_error(CONNECT, errno);
            exit(1);
        }
    }
    return sock;
}

int read_input(int fd, std::string& fill_me)
{
    char buffer[WA_MAX_INPUT];
    ssize_t byte_count = 0;
    int total_read = 0;
    char* temp = buffer;
    while (total_read < WA_MAX_INPUT)
    {
        byte_count = read(fd, temp, WA_MAX_INPUT - byte_count);
        if (byte_count > 0)
        {
            total_read += byte_count;
            temp += byte_count;
        }
        if (byte_count < 1)
        {
            return -1;
        }
    }
    std::string msg(buffer);
    msg.substr(0, '0');
    fill_me = msg;
    return total_read;
}

int write_output(int fd, std::string& message)
{
    char buffer[WA_MAX_INPUT];
    memset(buffer, 0, WA_MAX_INPUT);
    for(unsigned int i = 0; i < message.size(); ++i)
    {
        buffer[i] = message[i];
    }
    if(write(fd, buffer, WA_MAX_INPUT) < 0)
    {
        return -1;
    }
    return 0;
}


int indicate_peer(int fd, char& ind)
{
    if(write(fd, &ind, sizeof(char)) < 0)
    {
        return -1;
    }
    return 0;
}

char read_bit(int fd)
{
    char bit;
    if(read(fd, &bit, sizeof(char)) < 0)
    {
        print_error(READ, errno);
        exit(1);
    }
    return bit;
}

int char_to_int(char* convert_me)
{
    int result = 0;
    int len = strlen(convert_me);
    for (int i = 0; i < len; i++)
    {
        int temp = (convert_me[i] - '0');
        result += temp * pow(10, (len - (i + 1)));
    }
    return result;
}

void print_exit() {
    printf("EXIT command is typed: server is shutting down\n");
}

void print_connection() {
    printf("Connected Successfully.\n");
}

void print_connection_server(const std::string& client) {
    printf("%s connected.\n", client.c_str());
}

void print_dup_connection() {
    printf("Client name is already in use.\n");
}

void print_fail_connection() {
    printf("Failed to connect the server\n");
}

void print_server_usage() {
    printf("Usage: whatsappServer portNum\n");
}

void print_client_usage() {
    printf("Usage: whatsappClient clientName serverAddress serverPort\n");
}

void print_create_group(bool server, bool success, 
                        const std::string& client, const std::string& group) {
    if(server) {
        if(success) {
            printf("%s: Group \"%s\" was created successfully.\n", 
                   client.c_str(), group.c_str());
        } else {
            printf("%s: ERROR: failed to create group \"%s\"\n", 
                   client.c_str(), group.c_str());
        }
    }
    else {
        if(success) {
            printf("Group \"%s\" was created successfully.\n", group.c_str());
        } else {
            printf("ERROR: failed to create group \"%s\".\n", group.c_str());
        }
    }
}

void print_send(bool server, bool success, const std::string& client, 
                const std::string& name, const std::string& message) {
    if(server) {
        if(success) {
            printf("%s: \"%s\" was sent successfully to %s.\n", 
                   client.c_str(), message.c_str(), name.c_str());
        } else {
            printf("%s: ERROR: failed to send \"%s\" to %s.\n", 
                   client.c_str(), message.c_str(), name.c_str());
        }
    }
    else {
        if(success) {
            printf("Sent successfully.\n");
        } else {
            printf("ERROR: failed to send.\n");
        }
    }
}

void print_message(const std::string& client, const std::string& message) {
    printf("%s: %s\n", client.c_str(), message.c_str());
}

void print_who_server(const std::string& client) {
    printf("%s: Requests the currently connected client names.\n", client.c_str());
}

void print_who_client(bool success, const std::vector<std::string>& clients) {
    if(success) {
        bool first = true;
        for (const std::string& client: clients) {
            printf("%s%s", first ? "" : ",", client.c_str());
            first = false;
        }
        printf("\n");
    } else {
        printf("ERROR: failed to receive list of connected clients.\n");
    }
}

void print_exit(bool server, const std::string& client) {
    if(server) {
        printf("%s: Unregistered successfully.\n", client.c_str());
    } else {
        printf("Unregistered successfully.\n");
    }
}

void print_invalid_input() {
    printf("ERROR: Invalid input.\n");
}

void print_error(const std::string& function_name, int error_number) {
    printf("ERROR: %s %d.\n", function_name.c_str(), error_number);
}

void parse_command(const std::string& command, command_type& commandT, 
                   std::string& name, std::string& message, 
                   std::vector<std::string>& clients) {
    char c[WA_MAX_INPUT];
    const char *s; 
    char *saveptr;
    name.clear();
    message.clear();
    clients.clear();
    
    strcpy(c, command.c_str());
    s = strtok_r(c, " ", &saveptr);
    
    if(!strcmp(s, "create_group")) {
        commandT = CREATE_GROUP;
        s = strtok_r(NULL, " ", &saveptr);
        if(!s) {
            commandT = INVALID;
            return;
        } else {
            name = s;
            while((s = strtok_r(NULL, ",", &saveptr)) != NULL) {
                clients.emplace_back(s);
            }
        }
    } else if(!strcmp(s, "send")) {
        commandT = SEND;
        s = strtok_r(NULL, " ", &saveptr);
        if(!s) {
            commandT = INVALID;
            return;
        } else {
            name = s;
            message = command.substr(name.size() + 6); // 6 = 2 spaces + "send"
        }
    } else if(!strcmp(s, "who")) {
        commandT = WHO;
    } else if(!strcmp(s, "exit")) {
        commandT = EXIT;
    } else {
        commandT = INVALID;
    }
}
