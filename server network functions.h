#ifndef SERVER_NETWORK_
#define SERVER_NETWORK_

int id_read(char* buffer, int& pos) {
    int64_t ans = 0;
    while (buffer[pos]) {
        if (!('0' <= buffer[pos] && buffer[pos] <= '9')) return -1;
        ans *= 10, ans += buffer[pos++] - '0';
        if (ans >= INT_MAX) return -1;
    }
    return ans;
}


#endif