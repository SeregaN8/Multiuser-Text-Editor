#include <Windows.h>
#include <vector>
#include <iostream>
#include <cassert>
#include <fstream>
#include <ctime>
#include <map>
#include "common server-client functions.h"
#include "server network functions.h"
#include "implicit key tree.h"
const int PORT = 37641;
const int buffer_size = 10'000'000;
const std::clock_t disabling_time{60*CLOCKS_PER_SEC};

int main(int argc, char* argv[])
{
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSA startup failed\n";
        std::exit(1);
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0), new_socket;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char* buffer = new char[buffer_size]{ 0 };

    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Socket failed\n";
        std::exit(1);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, "", sizeof(opt))) {
        std::cerr << "Setsockopt failed\n";
        std::exit(1);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == INVALID_SOCKET) {
        std::cerr << "Bind failed\n";
        std::exit(1);
    }
    if (listen(server_fd, 100) < 0) {
        std::cerr << "Listen failed\n";
        std::exit(1);
    }

    int user_count{}, version{};
    treap* text;
    std::vector<std::pair<treap*, treap*>> reserved;
    std::vector<int> last_version_get;
    std::vector<char*> story;
    std::vector<std::clock_t> last_connection;
    std::clock_t last_check_run = std::clock();
    const std::string admin_key = generate_admin_key();
    std::map<std::string, int> name_to_index;
    std::vector<std::string> usernames;
    std::vector<int> disconnected;
    
    char* output_filename{};

    if (argc > 1) {
        std::ifstream textfile(argv[1]);
        int pos{};
        while (!textfile.eof())
        {
            textfile.get(buffer[pos++]);
        }
        textfile.close();
        buffer[pos++] = 0;

        text = init_from_buffer(buffer);
    }
    else {
        char* for_init = new char[2];
        for_init[0] = ' ', for_init[1] = 0;
        text = init_from_buffer(for_init);
    }
    if (argc > 2) output_filename = argv[2];
    else output_filename = "output.txt";


    std::cerr << "Admin key is: \n" << admin_key << '\n' << "Send it to person who you want to be an admin\n";
    for (;;) {
        if (std::clock_t current_time = std::clock(); current_time - last_check_run > disabling_time) {
            for (int i = 0; i < user_count; ++i) {
                if (!reserved[i].first) continue;
                if (current_time - last_connection[i] < disabling_time) continue;
                if (disconnected[i]) continue;
                int first = get_ind_by_pt(reserved[i].first), second = get_ind_by_pt(reserved[i].second);
                remove_reservation(first, second, text);
                disconnected[i] = 1;
            }
            last_check_run = current_time;
        }


        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }
        int r = recv(new_socket, buffer, buffer_size, 0);

        int pos = 0;
        std::string name = name_read(buffer, pos);
        if (name == "" || (name != admin_key && name_to_index.find(name) == name_to_index.end())) {
            send(new_socket, "Unknown name", 13, 0);
            std::cerr << "Unknown name call\n";
            closesocket(new_socket);
            continue;
        }

        if (name == admin_key) {
            if (buffer[pos] == 'a') {
                pos += 2;
                std::string to_add = name_read(buffer, pos);
                if (to_add == "") {
                    send(new_socket, "Incorrect name", 15, 0);
                }
                else {
                    send(new_socket, "Confirmed addition", 19, 0);
                    if (name_to_index.find(to_add) == name_to_index.end()) {
                        name_to_index[to_add] = user_count++;
                        usernames.push_back(to_add);
                        last_version_get.emplace_back(-1);
                        last_connection.emplace_back(0);
                        disconnected.emplace_back(0);
                        reserved.emplace_back(nullptr, nullptr);
                    }
                }
                std::cerr << "Addition command received\n";
            }
            else if (buffer[pos] == 's') {
                std::cerr << "Finish command received\n";
                closesocket(new_socket);
                break;
            }
            else {
                std::cerr << "Unknown command from admin\n";
            }
            closesocket(new_socket);
            continue;
        }

        int id = name_to_index[name];
        if (buffer[pos] == 'i') { //initiate
            int all_size = text->subtree_len;
            char* buffer_to_send = new char[all_size + 1];
            buffer_to_send[0] = 'o';

            write_tree_to_buffer(text, buffer_to_send + 1);

            send(new_socket, buffer_to_send, all_size + 1, 0);
            delete[] buffer_to_send;
            last_connection[id] = std::clock();
            last_version_get[id] = version;
            std::cerr << "initiate ended\n";
        }
        else if (buffer[pos] == 'c') { //check for new char
            if (id >= user_count || id == -1) {
                send(new_socket, "Invalid id, server doesn't know who you are", 11, 0);
            }
            else {
                if (last_version_get[id] == version) {
                    send(new_socket, "n", 1, 0);
                }
                else {
                    send(new_socket, "y", 1, 0);
                }
            }
            last_connection[id] = std::clock();
            std::cerr << "check ended\n";
        }
        else if (buffer[pos] == 'r') { //reserve
            int first{}, last{};
            if (last_version_get[id] < version) {
                send(new_socket, "o", 1, 0); //outdated
            }
            else {
                pos += 2;
                first = int_read(buffer, pos);
                last = int_read(buffer, pos);
                if (last < first) std::swap(first, last);
                auto p = check_reservation(first + 1, last + 1, text);
                if (p.first == -1) send(new_socket, "a", 1, 0), set_reservation(first + 1, last + 1, text, id),
                    reserved[id] = {get_pt_by_ind(text, first + 1), get_pt_by_ind(text, last + 1)};
                else {
                    char* to_send = new char[2 + usernames[p.first].length() + 1 + string_length(p.second - 1) + 1];
                    int pos = 0;
                    to_send[pos++] = 'r', to_send[pos++] = 0;
                    write_number(p.second - 1, to_send + pos), pos += string_length(p.second - 1) + 1;
                    memcpy(to_send + pos, usernames[p.first].c_str(), usernames[p.first].length() + 1);
                    pos += usernames[p.first].length() + 1;
                    send(new_socket, to_send, pos, 0);
                    delete[] to_send;
                }
            
            }
            last_connection[id] = std::clock();
            std::cerr << "reserve ended\n";
        }
        else if (buffer[pos] == 'e') { //edited
            pos += 2;
            bool critical_disconnection = disconnected[id] && (last_version_get[id] < version);
            while (last_version_get[id] < version) {
                char* to_send = story[last_version_get[id]];
                int len{};
                while (to_send[len]) ++len;
                ++len;
                while (to_send[len]) ++len;
                ++len;
                while (to_send[len]) ++len;
                ++len;
                send(new_socket, to_send, len, 0);
                char confirm[1];
                recv(new_socket, confirm, 1, 0);
                assert(confirm[0] == 'n');
                ++last_version_get[id];
            }
            send(new_socket, "a", 1, 0);
            std::cerr << "confirmed actuality\n";

            recv(new_socket, buffer, buffer_size, 0);
            int first = get_ind_by_pt(reserved[id].first), second = get_ind_by_pt(reserved[id].second);
            char* indexes = new char[string_length(first - 1) + 4 + string_length(second - 1)];
            indexes[1] = 0, pos = 2;
            write_number(first - 1, indexes + pos), pos += string_length(first - 1) + 1;
            write_number(second - 1, indexes + pos), pos += string_length(second - 1) + 1;
            if (critical_disconnection) {
                indexes[0] = 'n';
            }
            else {
                indexes[0] = 'y';
            }
            send(new_socket, indexes, pos, 0);
            delete[] indexes;

            if (critical_disconnection) {
                disconnected[id] = 0;
                reserved[id] = { nullptr, nullptr };
                last_connection[id] = std::clock();
                closesocket(new_socket);
                continue;
            }
            
            ++version, ++last_version_get[id];
            remove(text, first, second);

            treap* ins = init_from_buffer(buffer);
            insert_after(text, first, ins);

            char* for_story = new char[strlen(buffer) + 1 + string_length(first - 1) + string_length(second - 1) + 2];
            write_number(first - 1, for_story);
            write_number(second - 1, for_story + string_length(first - 1) + 1);
            memcpy(for_story + string_length(first-1) + string_length(second-1) + 2, buffer, strlen(buffer) + 1);
            story.emplace_back(for_story);
            
            reserved[id] = { nullptr, nullptr };
            last_connection[id] = std::clock();
            std::cerr << "edit ended\n";
        }
        else if (buffer[pos] == 'd') { //download
            pos += 2;
            while (last_version_get[id] < version) {
                char* to_send = story[last_version_get[id]];
                int len{};
                while (to_send[len]) ++len;
                ++len;
                while (to_send[len]) ++len;
                ++len;
                while (to_send[len]) ++len;
                ++len;
                send(new_socket, to_send, len, 0);
                char confirm[1];
                recv(new_socket, confirm, 1, 0);
                assert(confirm[0] == 'n');
                ++last_version_get[id];
            }
            send(new_socket, "a", 1, 0);
            last_connection[id] = std::clock();
        }
        else {
            send(new_socket, "Unknown command", 16, 0);
        }
        closesocket(new_socket);
    }

    std::ofstream ofs(output_filename);
    write_to_file(text, ofs);
    ofs.close();

    shutdown(server_fd, 2);
    return 0;
}