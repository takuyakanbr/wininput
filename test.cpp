
#include "stdafx.h"

#include <windows.h>
#include <iostream>

#define _WININPUT_DEBUG
#include "src/wininput.cpp"

namespace {
	volatile int running = 1;

	input::KeyData seq[6];

	bool keyHandler(input::KeyData& data) {
		if (data.type == INPUT_TYPE_KEYDOWN)
			std::cout << "K: " << data.vkCode << std::endl;
		return false;
	}

	bool mouseHandler(input::MouseData& data) {
		if (data.code != WM_MOUSEMOVE)
			std::cout << "M: " << data.code << " [" << data.x << ", " << data.y << "]" << std::endl;
		return false;
	}

	bool seqMatchHandler() {
		std::cout << "Hello matched." << std::endl;
		return false;
	}

	BOOL WINAPI handlerRoutine(DWORD signum) {
		std::cout << "Received control signal: " << signum << std::endl;
		running = 0;
		Sleep(10000);
		return TRUE;
	}
}

int main() {
	std::cout << "Main started." << std::endl;

	SetConsoleCtrlHandler(handlerRoutine, TRUE);

	input::addKeyHandler(keyHandler);
	input::addMouseHandler(mouseHandler);

	seq[0] = { 0x48, false, true, false, 3 };
	seq[1] = { 0x45, false, false, false, 3 };
	seq[2] = { 0x4C, false, false, false, 3 };
	seq[3] = { 0x4C, false, false, false, 3 };
	seq[4] = { 0x4F, false, false, false, 3 };
	int handlerId;
	input::onKeyEvent(seq, true, seqMatchHandler, &handlerId);

	while (running) {
		Sleep(50);
	}

	input::shutdown();
	std::cout << "Main ended." << std::endl;

	return 0;
}

