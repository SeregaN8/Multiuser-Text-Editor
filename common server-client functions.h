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

#endif COMMON_SC_LOGICS_
