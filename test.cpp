
#include "stdafx.h"

#include <windows.h>
#include <iostream>

#define _WININPUT_DEBUG
#include "src/wininput.cpp"

namespace {
	volatile int running = 1;

	input::KeyData seq1[6];
	input::KeyData seq2[6];

	bool keyHandler(input::KeyData& data) {
		if (data.type == INPUT_TYPE_KEYDOWN)
			std::cout << "K: " << data.code << std::endl;
		return false;
	}

	bool mouseHandler(input::MouseData& data) {
		if (data.code != WM_MOUSEMOVE)
			std::cout << "M: " << data.code << " [" << data.x << ", " << data.y << "]" << std::endl;
		return false;
	}

	bool seq1MatchHandler() {
		std::cout << "Ctrl+Shift+4 matched." << std::endl;
		return false;
	}

	bool seq2MatchHandler() {
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

	seq1[0] = { 0x34, true, true, false, 3 }; // Ctrl+Shift+4
	seq2[0] = { 0x48, false, true, false, 3 }; // Hello
	seq2[1] = { 0x45, false, false, false, 3 };
	seq2[2] = { 0x4C, false, false, false, 3 };
	seq2[3] = { 0x4C, false, false, false, 3 };
	seq2[4] = { 0x4F, false, false, false, 3 };
	int seqId;
	input::addKeySequence(seq1, true, seq1MatchHandler, &seqId);
	input::addKeySequence(seq2, true, seq2MatchHandler, &seqId);

	while (running) {
		Sleep(50);
	}

	input::shutdown();
	std::cout << "Main ended." << std::endl;

	return 0;
}

