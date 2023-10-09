#include <windows.h>
#include <random>
#include <chrono>
#include <fstream>
#include <cassert>
#include <string>
#include "common server-client functions.h"
const int PORT = 37641;
const int buffer_size = 10'000'000;

HWND MainHwn, hList, hEdit, hReserve, hDownload, hSend, hStop;
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

int identificator_len;
char* str_identificator = nullptr;
unsigned long ip_to_connect{};
std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

int MRegister(const char* ClassName, WNDPROC wndproc)
{
	WNDCLASSEX   wndClass;
	memset(&wndClass, 0, sizeof(WNDCLASSEX));
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpfnWndProc = wndproc;
	wndClass.hInstance = GetModuleHandle(NULL);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.style = 0;
	wndClass.lpszMenuName = ClassName;
	wndClass.lpszClassName = ClassName;
	return RegisterClassEx(&wndClass);
}

int static_wsadata_initialization() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		MessageBox(MainHwn, "WSAStartup failed with some error", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		std::exit(1);
	}

	std::ifstream ipfile("ip.txt");
	if (!ipfile.is_open()) {
		MessageBox(MainHwn, "Invalid file, the programme will select default ip value (127.0.0.1)", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		ip_to_connect = 0x7f000001;
		return 1;
	}
	unsigned int read{};
	char fictive;
	for (int i = 0; i < 4; ++i) {
		ipfile >> read;
		if (read > 255) {
			MessageBox(MainHwn, "Incorrect ip, the programme will select default ip value (127.0.0.1)", "error", MB_OKCANCEL | MB_ICONINFORMATION);
			ip_to_connect = 0x7f000001;
			ipfile.close();
			return 1;
		}
		for (int j = 0; j < 3 - i; ++j) read *= 256;
		ip_to_connect += read;
		if (i < 3) ipfile >> fictive;
	}
	ipfile.close();
	return 1;
}

SOCKET create_connection() {
	static int fictive_var = static_wsadata_initialization();

	int status;
	SOCKET client_fd;
	sockaddr_in serv_addr;
	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		MessageBox(MainHwn, "Socket creation error", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		std::exit(1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	serv_addr.sin_addr.s_addr = htonl(ip_to_connect);

	if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
		MessageBox(MainHwn, "Connection Failed", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		std::exit(1);
	}
	return client_fd;
}

void initData()
{
	SOCKET client_fd = create_connection();

	send(client_fd, "i", 1, 0);

	char* buffer = new char[buffer_size] { 0 };
	recv(client_fd, buffer, buffer_size, 0);

	closesocket(client_fd);

	size_t begin_pos{}, end_pos{};
	while (buffer[end_pos]) ++identificator_len, ++end_pos;
	str_identificator = new char[++identificator_len];
	while (buffer[begin_pos]) str_identificator[begin_pos] = buffer[begin_pos], ++begin_pos;
	begin_pos = ++end_pos, str_identificator[identificator_len - 1] = 0;

	while (buffer[begin_pos]) {
		while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
		int val{}, r_pos = end_pos;
		while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
		char* detached_line = new char[end_pos + 1 - begin_pos];
		size_t write_pos{};
		while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
		detached_line[write_pos] = 0;
		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)detached_line);

		if (buffer[end_pos] == 0) break;
		++end_pos, begin_pos = end_pos;
		delete[] detached_line;
	}
	delete[] buffer;
}

bool approved_new_changes() {
	SOCKET client_fd = create_connection();
	char* send_buf = new char[identificator_len + 2];
	memcpy(send_buf + 2, str_identificator, identificator_len);
	send_buf[0] = 'c', send_buf[1] = 0;
	send(client_fd, send_buf, identificator_len + 2, 0);

	char buffer[20];

	recv(client_fd, buffer, 20, 0);
	bool ret = (buffer[0] == 'y');
	delete[] send_buf;
	closesocket(client_fd);
	return ret;
}

void download_new_changes() {
	SOCKET client_fd = create_connection();
	char* send_buf = new char[identificator_len + 2];
	memcpy(send_buf + 2, str_identificator, identificator_len);
	send_buf[0] = 'd', send_buf[1] = 0;
	send(client_fd, send_buf, identificator_len + 2, 0);

	char* buffer = new char[buffer_size] { 0 };
	recv(client_fd, buffer, buffer_size, 0);
	while (buffer[0] != 'a') {
		int first = 0, last = 0;
		size_t pos = 0;
		while (buffer[pos]) first *= 10, first += buffer[pos++] - '0';
		++pos;
		while (buffer[pos]) last *= 10, last += buffer[pos++] - '0';
		++pos;
		if (first > last) std::swap(first, last);

		for (int i = first; i <= last; ++i) SendMessage(hList, LB_DELETESTRING, first, 0);

		size_t begin_pos{ pos }, end_pos{pos};
		int ind{};

		while (buffer[begin_pos]) {
			while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
			int val{}, r_pos = end_pos;
			while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
			char* detached_line = new char[end_pos + 1 - val -  begin_pos];
			size_t write_pos{};
			while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
			detached_line[write_pos] = 0;
			SendMessage(hList, LB_INSERTSTRING, first + ind, (LPARAM)detached_line);

			if (buffer[end_pos] == 0) break;
			begin_pos = ++end_pos, ++ind;
			delete[] detached_line;
		}
		send(client_fd, "n", 1, 0);
		recv(client_fd, buffer, buffer_size, 0);
	}
	delete[] buffer;
	delete[] send_buf;

	closesocket(client_fd);
}

int reserve(int first, int second) { //1 if successful, 0 if reserved by someone, -1 if not actual version
	SOCKET client_fd = create_connection();
	char* send_buf = new char[2 + identificator_len + string_length(first) + string_length(second) + 2];
	send_buf[0] = 'r', send_buf[1] = 0;
	memcpy(send_buf + 2, str_identificator, identificator_len);
	write_number(first, send_buf + 2 + identificator_len);
	write_number(second, send_buf + 2 + identificator_len + 1 + string_length(first));
	send(client_fd, send_buf, identificator_len + 2 + 2 + string_length(first) + string_length(second), 0);

	char buffer[20];
	recv(client_fd, buffer, 20, 0);
	closesocket(client_fd);
	delete[] send_buf;

	if (buffer[0] == 'a') {
		return 1;
	}
	if (buffer[0] == 'r') {
		return 0;
	}
	return -1;
}

void send_changes() { //firstly send changes, then downloads changes, user own changes may be not transferred twice over the net
	SOCKET client_fd = create_connection();
	char* send_buf = new char[identificator_len + 2];
	memcpy(send_buf + 2, str_identificator, identificator_len);
	send_buf[0] = 'e', send_buf[1] = 0;
	send(client_fd, send_buf, identificator_len + 2, 0);

	char* buffer = new char[buffer_size] { 0 };
	recv(client_fd, buffer, buffer_size, 0);
	while (buffer[0] != 'a') {
		int first = 0, last = 0;
		size_t pos = 0;
		while (buffer[pos]) first *= 10, first += buffer[pos++] - '0';
		++pos;
		while (buffer[pos]) last *= 10, last += buffer[pos++] - '0';
		++pos;

		for (int i = first; i <= last; ++i) SendMessage(hList, LB_DELETESTRING, first, 0);

		size_t begin_pos{ pos }, end_pos{ pos };
		int ind{};

		while (buffer[begin_pos]) {
			while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
			int val{}, r_pos = end_pos;
			while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
			char* detached_line = new char[end_pos + 1 - val - begin_pos];
			size_t write_pos{};
			while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
			detached_line[write_pos] = 0;
			SendMessage(hList, LB_INSERTSTRING, first + ind, (LPARAM)detached_line);

			if (buffer[end_pos] == 0) break;
			begin_pos = ++end_pos, ++ind;
			delete[] detached_line;
		}
		send(client_fd, "n", 1, 0);
		recv(client_fd, buffer, buffer_size, 0);
	}
	GetWindowText(hEdit, buffer, buffer_size);
	int len = GetWindowTextLength(hEdit);
	buffer[len] = 0;
	send(client_fd, buffer, len + 1, 0);
	delete[] buffer;

	closesocket(client_fd);
}

void extract_text(int first, int second) {
	if (first > second) std::swap(first, second);
	size_t text_length{};
	for (int i = first; i <= second; ++i) {
		text_length += SendMessage(hList, LB_GETTEXTLEN, i, 0) + 2;
	}
	char* line_for_edit = new char[text_length];
	int pos = 0;
	for (int i = first; i <= second; ++i) {
		SendMessage(hList, LB_GETTEXT, i, (LPARAM)(line_for_edit + pos));
		pos += SendMessage(hList, LB_GETTEXTLEN, i, 0);
		line_for_edit[pos++] = '\r';
		line_for_edit[pos++] = '\n';
	}
	line_for_edit[pos - 1] = 0;
	SetWindowText(hEdit, line_for_edit);
	delete[] line_for_edit;
}

void local_insert(int first, int last, char* str) {
	if (first > last) std::swap(first, last);
	for (int i = first; i <= last; ++i) SendMessage(hList, LB_DELETESTRING, first, 0);

	size_t begin_pos{}, end_pos{};
	int ind{};

	while (str[begin_pos]) {
		while (str[end_pos] != '\n' && str[end_pos] != 0) ++end_pos;
		int val{}, r_pos = end_pos;
		while (r_pos > begin_pos && str[r_pos - 1] == '\r') ++val, --r_pos;
		char* detached_line = new char[end_pos + 1 - val - begin_pos];
		size_t write_pos{};
		while (begin_pos < end_pos - val) detached_line[write_pos++] = str[begin_pos++];
		detached_line[write_pos] = 0;
		SendMessage(hList, LB_INSERTSTRING, first + ind, (LPARAM)detached_line);

		if (str[end_pos] == 0) break;
		begin_pos = ++end_pos, ++ind;
		delete[] detached_line;
	}
}

void stop_function() {
	SOCKET client_fd = create_connection();
	send(client_fd, "s", 1, 0);
	closesocket(client_fd);
	std::exit(0);
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	char* szAppName = "WinApiSample";
	MRegister(szAppName, WndProc);

	MainHwn = CreateWindow(szAppName, "API Sample", WS_MAXIMIZEBOX | WS_VISIBLE | WS_SYSMENU,
		100, 50, 900, 600, HWND_DESKTOP, NULL, hInstance, NULL); 
	hList = CreateWindow("LISTBOX", "",
		WS_BORDER | WS_VISIBLE | WS_CHILD | LBS_NOTIFY | WS_VSCROLL, 5, 50, 880, 250, MainHwn,
		(HMENU)2, hInstance, NULL);
	hEdit = CreateWindow("EDIT", "",
		WS_BORDER | WS_VISIBLE | ES_MULTILINE | WS_CHILD | WS_VSCROLL | ES_AUTOHSCROLL |
		WS_HSCROLL, 5, 310, 880, 200, MainHwn, NULL, hInstance, NULL);
	hReserve = CreateWindow("BUTTON", "Reserve",
		WS_BORDER | WS_CHILD, 10, 530, 120, 30, MainHwn, (HMENU)101, hInstance, NULL);
	hDownload = CreateWindow("BUTTON", "Download changes",
		WS_BORDER | WS_CHILD, 720, 530, 150, 30, MainHwn, (HMENU)102, hInstance, NULL);
	hSend = CreateWindow("BUTTON", "Send changes",
		WS_BORDER | WS_CHILD, 420, 530, 150, 30, MainHwn, (HMENU)103, hInstance, NULL);
	hStop = CreateWindow("BUTTON", "Finish editing",
		WS_BORDER | WS_VISIBLE |  WS_CHILD, 400, 10, 100, 30, MainHwn, (HMENU)104, hInstance, NULL);
	UpdateWindow(MainHwn);
	initData();
	ShowWindow(hReserve, 5);
	SetTimer(MainHwn, 1, 10'000 + rng() % 1000, 0);

	MSG  msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static bool something_is_editing{};
	static bool was_selected{}, was_selected_with_shift{};
	static int Lfirst{}, Lsecond{};

	switch (Message) {

	case WM_DESTROY: PostQuitMessage(0);
		break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		RECT rc;
		GetClientRect(MainHwn, &rc);
		HDC hdc = BeginPaint(MainHwn, &ps);
		FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
		EndPaint(MainHwn, &ps);
	} break;

	case WM_TIMER:
		if (approved_new_changes()) {
			ShowWindow(hDownload, 5);
			ShowWindow(hReserve, 0);
			UpdateWindow(MainHwn);
		}
		break;

	case WM_COMMAND:
		switch (wParam) { // window code
		case 101: {//hreserve
			if (!(was_selected && was_selected_with_shift)) break;
			if (something_is_editing) break;
			int res = reserve(Lfirst, Lsecond);
			if (res == 1) {
				was_selected = was_selected_with_shift = false;
				something_is_editing = true;
				ShowWindow(hReserve, 0);
				ShowWindow(hReserve, 0);
				ShowWindow(hSend, 5);
				ShowWindow(hDownload, 0);
				UpdateWindow(MainHwn);
				extract_text(Lfirst, Lsecond);
			}
			else if (res == 0) {
				MessageBox(MainHwn, "Lines you try to take for edit are already reserved!", "error", MB_OKCANCEL | MB_ICONINFORMATION);
			}
			else {
				ShowWindow(hDownload, 5);
				ShowWindow(hReserve, 0);
				UpdateWindow(MainHwn);
				was_selected = was_selected_with_shift = false;
				MessageBox(MainHwn, "Your version is not current server version, please download changes to exit text", "error", MB_OKCANCEL | MB_ICONINFORMATION);
			}
			break;
		}
		case 102: //hdownload
		{
			download_new_changes();
			ShowWindow(hDownload, 0);
			if (!something_is_editing) ShowWindow(hReserve, 5);
			UpdateWindow(MainHwn);
			break;
		}
		case 103: {//hSend
			send_changes();
			ShowWindow(hReserve, 5);
			ShowWindow(hSend, 0);
			ShowWindow(hDownload, 0);
			UpdateWindow(MainHwn);
			char* buffer = new char[buffer_size];
			GetWindowText(hEdit, buffer, buffer_size);
			local_insert(Lfirst, Lsecond, buffer);
			delete[] buffer;
			SetWindowText(hEdit, "");

			something_is_editing = false;
			break;
		}
		case 104: {
			stop_function();
			break;
		}
		default:
			if (LOWORD(wParam) == 2) {
				if (something_is_editing) break;
				if (GetKeyState(VK_LSHIFT) < 0) {
					was_selected_with_shift = true;
					Lsecond = SendMessage(hList, LB_GETCURSEL, 0, 0);
				}
				else if(GetKeyState(VK_RSHIFT) < 0) {
					was_selected = true;
					Lfirst = SendMessage(hList, LB_GETCURSEL, 0, 0);
				}
				GetTickCount();
			}
			break;

		}
	}
	return DefWindowProc(hwnd, Message, wParam, lParam);
}