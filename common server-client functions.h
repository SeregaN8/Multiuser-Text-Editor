#ifndef COMMON_SC_LOGICS_
#define COMMON_SC_LOGICS_

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

int int_read(char* buffer, int& pos) {
    int64_t ans = 0;
    while (buffer[pos]) {
        if (!('0' <= buffer[pos] && buffer[pos] <= '9')) return -1;
        ans *= 10, ans += buffer[pos++] - '0';
        if (ans >= INT_MAX) return -1;
    }
    ++pos;
    return ans;
}

#endif COMMON_SC_LOGICS_
