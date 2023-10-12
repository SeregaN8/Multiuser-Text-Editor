#ifndef SERVER_NETWORK_
#define SERVER_NETWORK_

#include <string>

std::string name_read(char* buf, int& pos) {
    std::string ans;
    while (buf[pos]) {
        if (buf[pos] <= 32 || buf[pos] > 126) return "";
        ans.push_back(buf[pos++]);
        if (pos > 50) return "";
    }
    ++pos;
    return ans;
}

#endif