#include <Windows.h>
#include <vector>
#include <iostream>
#include <cassert>
#include <fstream>
#define PORT 37641
#define buffer_size 10'000'000

struct for_save_before_delete {
    std::ofstream ofs;
    std::vector<const char*>* pt_to_text;
    for_save_before_delete(char* filename, std::vector<const char*>* ptt) : ofs(filename), pt_to_text(ptt) {}

    ~for_save_before_delete() {
        for (auto c : *pt_to_text) {
            while (*c) ofs << *c, ++c;
            ofs << "\n";
        }
    }
};

int string_length(int num) {
    if (num == 0) return 1;
    int ans{};
    while (num) ++ans, num /= 10;
    return ans;
}

void write_number(int num, char* place) {
    int length = string_length(num);
    place[length] = 0;
    for (int i = length - 1; i >= 0; --i) place[i] = num % 10 + '0', num /= 10;
}

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


    int user_count{}, common_text_length = 2, version{};
    std::vector<const char*> text{};
    std::vector<int> last_version_get;
    std::vector<int> who_reserved{ -1 };
    std::vector<char*> story;
    char* output_filename{};

    if (argc > 1) {
        std::ifstream textfile(argv[1]);
        int pos{};
        while (!textfile.eof())
        {
            textfile.get(buffer[pos++]);
        }
        textfile.close();

        size_t begin_pos{}, end_pos{};

        while (buffer[begin_pos]) {
            while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
            int val{}, r_pos = end_pos;
            while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
            char* detached_line = new char[end_pos + 1 - val - begin_pos];
            size_t write_pos{};
            while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
            detached_line[write_pos] = 0;
            text.emplace_back(detached_line);
            who_reserved.emplace_back( -1);
            common_text_length += strlen(detached_line) + 1;

            if (buffer[end_pos] == 0) break;
            begin_pos = ++end_pos;
        }

        if (argc > 2) output_filename = argv[2];
        else output_filename = "output.txt";
    }
    else {
        char* for_init = new char[2];
        for_init[0] = ' ', for_init[1] = 0;
        text.emplace_back(for_init);
    }

    for_save_before_delete fictive_var(output_filename, &text);


    for (;;) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }
        int r = recv(new_socket, buffer, buffer_size, 0);

        if (buffer[0] == 'i') { //initiate
            int all_size = string_length(user_count) + common_text_length + 1;
            char* buffer_to_send = new char[all_size];
            write_number(user_count, buffer_to_send);
            int already_wrote = string_length(user_count++) + 1;
            for (const char* line : text) {
                int len = strlen(line);
                memcpy(buffer_to_send + already_wrote, line, len);
                buffer_to_send[already_wrote + len] = '\n';
                already_wrote += (len + 1);
            }
            buffer_to_send[all_size - 1] = 0;
            send(new_socket, buffer_to_send, all_size, 0);
            delete[] buffer_to_send;
            last_version_get.emplace_back(version);
            std::cerr << "initiate ended\n";
        }
        else if (buffer[0] == 'c') { //check for new char
            int id = 0, pos = 2;
            while (buffer[pos]) id *= 10, id += buffer[pos++] - '0';
            if (id >= user_count) {
                send(new_socket, "who are you", 11, 0);
            }
            else {
                if (last_version_get[id] == version) {
                    send(new_socket, "n", 1, 0);
                }
                else {
                    send(new_socket, "y", 1, 0);
                }
            }
            std::cerr << "check ended\n";
        }
        else if (buffer[0] == 'r') { //reserve
            int id = 0, pos = 2;
            int first{}, last{};
            while (buffer[pos]) id *= 10, id += buffer[pos++] - '0';
            if (id >= user_count) {
                send(new_socket, "who are you", 11, 0);
            }
            else if (last_version_get[id] < version) {
                send(new_socket, "o", 1, 0); //outdated
            }
            else {
                bool solution = true;
                ++pos;
                while (buffer[pos]) first *= 10, first += buffer[pos++] - '0';
                ++pos;
                while (buffer[pos]) last *= 10, last += buffer[pos++] - '0';
                if (last < pos) std::swap(first, last);
                for (int i = first; i <= last; ++i) {
                    if (who_reserved[i] != -1) {
                        solution = false;
                        break;
                    }
                }
                if (solution) {
                    send(new_socket, "a", 1, 0); //accepted
                    for (int i = first; i <= last; ++i) {
                        who_reserved[i] = id;
                    }
                }
                else {
                    send(new_socket, "r", 1, 0); //rejected
                }
            }
            std::cerr << "reserve ended\n";
        }
        else if (buffer[0] == 'e') { //edited
            int id = 0, pos = 2;
            while (buffer[pos]) id *= 10, id += buffer[pos++] - '0';
            if (id >= user_count) {
                send(new_socket, "who are you", 11, 0);
            }
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
            int first = INT_MAX, second = -1;
            ++version, ++last_version_get[id];
            for (int i = 0; i < who_reserved.size(); ++i) {
                if (who_reserved[i] == id) {
                    first = min(first, i), second = max(second, i);
                }
            }
            for (int i = second; i >= first; --i) {
                common_text_length -= strlen(text[i]) + 1;
                delete[] text[i];
                text.erase(text.begin() + i);
                who_reserved.erase(who_reserved.begin() + i);
            }

            size_t begin_pos{}, end_pos{};
            int ind{};

            while (buffer[begin_pos]) {
                while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
                int val{}, r_pos = end_pos;
                while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
                char* detached_line = new char[end_pos + 1 - val - begin_pos];
                size_t write_pos{};
                while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
                detached_line[write_pos] = 0;
                text.insert(text.begin() + first + ind, detached_line);
                who_reserved.insert(who_reserved.begin() + first + ind, -1);
                common_text_length += strlen(detached_line) + 1;

                if (buffer[end_pos] == 0) break;
                begin_pos = ++end_pos, ++ind;
            }

            char* for_story = new char[strlen(buffer) + 1 + string_length(first) + string_length(second) + 2];
            write_number(first, for_story);
            write_number(second, for_story + string_length(first) + 1);
            memcpy(for_story + string_length(first) + string_length(second) + 2, buffer, strlen(buffer) + 1);
            story.emplace_back(for_story);

            std::cerr << "edit ended\n";
        }
        else if (buffer[0] == 'd') { //download
            int id = 0, pos = 2;
            while (buffer[pos]) id *= 10, id += buffer[pos++] - '0'; 
            if (id >= user_count) {
                send(new_socket, "who are you", 11, 0);
            }
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
        }
        else if (buffer[0] == 's') {
            closesocket(new_socket);
            break;
        }
        closesocket(new_socket);
    }

    shutdown(server_fd, 2);
    return 0;
}
