#include <windows.h>
#include <random>
#include <chrono>
#include <thread>
#include <fstream>
#include <cassert>
#include <sstream>
#include <string>
#include "common server-client functions.h"
const int PORT = 37641;
const int buffer_size = 10'000'000;

HWND MainHwn, hList, hEdit, hReserve, hDownload, hSend;
LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM);

unsigned long ip_to_connect{};
std::string username;
std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());

void ResizeMainWindow()
{
	RECT rc;
	GetClientRect(MainHwn, &rc);
	SetWindowPos(hList, HWND_TOP, 5, rc.bottom/12, rc.right - 10, 5*rc.bottom/12, SWP_NOZORDER);
	SetWindowPos(hEdit, HWND_TOP, 5, rc.bottom/2 + 10, rc.right - 10, rc.bottom/3, SWP_NOZORDER);
	SetWindowPos(hReserve, HWND_TOP, 10, 11 * rc.bottom / 12, 120, 30, SWP_NOZORDER);
	SetWindowPos(hDownload, HWND_TOP, 4*rc.right/5, 11*rc.bottom/12, 150, 30, SWP_NOZORDER);
	SetWindowPos(hSend, HWND_TOP, rc.right/2, 11*rc.bottom/12, 150, 30, SWP_NOZORDER);
}

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
	if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	serv_addr.sin_addr.s_addr = htonl(ip_to_connect);

	if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
		return INVALID_SOCKET;
	}
	return client_fd;
}

int initData()
{
	static int failed_init = 0;
	SOCKET client_fd = create_connection();

	if (client_fd == INVALID_SOCKET) {
		if (!failed_init) {
			int ret = MessageBox(MainHwn, "Failed to connect to server, do you want to wait?", "error", MB_OKCANCEL | MB_ICONINFORMATION);
			if (ret == IDOK) {
				failed_init = 1;
				return 2;
			}
			std::exit(0);
		}
		return 2;
	}

	char* to_send = new char[username.length() + 3];
	int pos = 0;
	memcpy(to_send, username.c_str(), username.length() + 1), pos += username.length() + 1;
	to_send[pos++] = 'i', to_send[pos++] = 0;
	send(client_fd, to_send, pos, 0);
	delete[] to_send;

	char* buffer = new char[buffer_size] { 0 };
	recv(client_fd, buffer, buffer_size, 0);

	closesocket(client_fd);

	if (*buffer == 'U') {
		MessageBox(MainHwn, "Incorrect name, client don't have access to server", "Error", MB_OK | MB_ICONINFORMATION);
		std::exit(0);
	}
	++buffer;

	size_t begin_pos{}, end_pos{};;

	while (buffer[begin_pos]) {
		while (buffer[end_pos] != '\n' && buffer[end_pos] != 0) ++end_pos;
		int val{}, r_pos = end_pos;
		while (r_pos > begin_pos && buffer[r_pos - 1] == '\r') ++val, --r_pos;
		char* detached_line = new char[end_pos + 1 - val - begin_pos];
		size_t write_pos{};
		while (begin_pos + val < end_pos) detached_line[write_pos++] = buffer[begin_pos++];
		detached_line[write_pos] = 0;
		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)detached_line);

		if (buffer[end_pos] == 0) break;
		begin_pos = ++end_pos;
		delete[] detached_line;
	}
	--buffer;
	delete[] buffer;
	return 0;
}

int approved_new_changes() {
	static int confirmed_network_troubles = 0;
	SOCKET client_fd = create_connection();
	if (client_fd == INVALID_SOCKET) {
		if (!confirmed_network_troubles) {
			int ret = MessageBox(MainHwn, "It looks like the server is currently unavailable, do you want to wait?", "error", MB_OKCANCEL | MB_ICONINFORMATION);
			if (ret == IDOK) {
				confirmed_network_troubles = 1;
				return 2;
			}
			std::exit(0);
		}
		return 2;
	}
	else {
		if (confirmed_network_troubles) {
			MessageBox(MainHwn, "Connection restored", "Notification", MB_OKCANCEL | MB_ICONINFORMATION);
			confirmed_network_troubles = 0;
		}
	}

	int pos = 0;
	char* send_buf = new char[username.length() + 3];
	memcpy(send_buf, username.c_str(), username.length() + 1), pos += username.length() + 1;
	send_buf[pos++] = 'c', send_buf[pos++] = 0;
	send(client_fd, send_buf, pos, 0);

	char buffer[20];

	recv(client_fd, buffer, 20, 0);
	bool ret = (buffer[0] == 'y');
	delete[] send_buf;
	closesocket(client_fd);
	return ret;
}

int download_new_changes() {
	SOCKET client_fd = create_connection();
	if (client_fd == INVALID_SOCKET) {
		int ret = MessageBox(MainHwn, "Failed to connect to server, do you want to wait?", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		if (ret == IDOK) {
			return 2;
		}
		std::exit(0);
	}
	int pos = 0;
	char* send_buf = new char[username.length() + 3];
	memcpy(send_buf, username.c_str(), username.length() + 1), pos += username.length() + 1;
	send_buf[pos++] = 'd', send_buf[pos++] = 0;
	send(client_fd, send_buf, pos, 0);

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
	return 0;
}

int reserve(int first, int second) { //1 if successful, 0 if reserved by someone, -1 if not actual version
	SOCKET client_fd = create_connection();
	if (client_fd == INVALID_SOCKET) {
		int ret = MessageBox(MainHwn, "Failed to connect to server, do you want to wait?", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		if (ret == IDOK) {
			return 2;
		}
		std::exit(0);
	}
	int pos = 0;
	char* send_buf = new char[username.length() + 5 + string_length(first) + string_length(second)];
	memcpy(send_buf, username.c_str(), username.length() + 1), pos += username.length() + 1;
	send_buf[pos++] = 'r', send_buf[pos++] = 0;
	write_number(first, send_buf + pos), pos += 1 + string_length(first);
	write_number(second, send_buf + pos), pos += 1 + string_length(second);
	send(client_fd, send_buf, pos, 0);

	char buffer[100];
	recv(client_fd, buffer, 100, 0);
	closesocket(client_fd);
	delete[] send_buf;

	if (buffer[0] == 'a') {
		return 1;
	}
	if (buffer[0] == 'r') {
		std::string ind(buffer + 2);
		std::string reservist(buffer + 3 + ind.length());
		std::string message;
		message += "Line ";
		message += ind;
		message += " is currently reserved by user ";
		message += reservist;
		message.push_back('!');
		MessageBox(MainHwn, message.c_str(), "Error", MB_OK | MB_ICONINFORMATION);
		return 0;
	}
	return -1;
}

int send_changes(int& first, int& last) { //firstly send changes, then downloads changes, user own changes may be not transferred twice over the net
	SOCKET client_fd = create_connection();
	if (client_fd == INVALID_SOCKET) {
		int ret = MessageBox(MainHwn, "Failed to connect to server, do you want to wait?", "error", MB_OKCANCEL | MB_ICONINFORMATION);
		if (ret == IDOK) {
			return 2;
		}
		std::exit(0);
	}
	int pos = 0;
	char* send_buf = new char[username.length() + 3];
	memcpy(send_buf, username.c_str(), username.length() + 1), pos += username.length() + 1;
	send_buf[pos++] = 'e', send_buf[pos++] = 0;
	send(client_fd, send_buf, pos, 0);

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
	recv(client_fd, buffer, buffer_size, 0);
	if (buffer[0] == 'n') {
		first = last = -1;
		delete[] buffer;

		closesocket(client_fd);
		return 0;
	}
	pos = 2;
	first = int_read(buffer, pos);
	last = int_read(buffer, pos);
	delete[] buffer;

	closesocket(client_fd);
	return 0;
}

void extract_text(int first, int second) {
	if (first > second) std::swap(first, second);
	size_t text_length{1};
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
	line_for_edit[pos] = 0;
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

std::string get_username(const char* pt) {
	std::string ans;
	while (32 < *pt && *pt < 127) {
		ans.push_back(*pt), pt++;
	}
	if (ans.empty()) {
		MessageBox(MainHwn, "Not enough arguments: username expected", "Error", MB_OKCANCEL | MB_ICONINFORMATION);
		std::exit(0);
	}
	return ans;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	char* szAppName = "WinApiSample";
	MRegister(szAppName, WndProc);
	username = get_username(lpszCmdLine);

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
	UpdateWindow(MainHwn);
	while (initData() == 2) {
		std::this_thread::sleep_for(std::chrono::milliseconds(3'000));
	}
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
		if (approved_new_changes() == 1) {
			ShowWindow(hDownload, 5);
			ShowWindow(hReserve, 0);
			UpdateWindow(MainHwn);
		}
		break;

	case WM_SIZE:
		ResizeMainWindow();
		break;

	case WM_COMMAND:
		switch (wParam) { // window code
		case 101: {//hreserve
			if (!(was_selected && was_selected_with_shift)) break;
			if (something_is_editing) break;
			int res = reserve(Lfirst, Lsecond);
			if (res == 2) return 0;
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
			else if (res == -1){
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
			if (download_new_changes() == 2) return 0;
			ShowWindow(hDownload, 0);
			if (!something_is_editing) ShowWindow(hReserve, 5);
			UpdateWindow(MainHwn);
			break;
		}
		case 103: {//hSend
			int a = Lfirst, b = Lsecond;
			if (send_changes(a, b) == 2) return 0;
			ShowWindow(hReserve, 5);
			ShowWindow(hSend, 0);
			ShowWindow(hDownload, 0);
			UpdateWindow(MainHwn);
			if (a == -1) {
				MessageBox(MainHwn, "You've been disconnected from server for too long, your changes "
					"were rejected", "error", MB_OK | MB_ICONINFORMATION);
				return 0;
			}
			char* buffer = new char[buffer_size];
			GetWindowText(hEdit, buffer, buffer_size);
			local_insert(a, b, buffer);
			delete[] buffer;
			SetWindowText(hEdit, "");

			something_is_editing = false;
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
