
#include "wininput.hpp"

#include <iostream>
#include <list>
#include <mutex>
#include <windows.h>

#ifdef _WININPUT_DEBUG
#define _D(x) std::cout << x
#else
#define _D(x)
#endif

namespace {

	struct KeySequence {
		int id;
		int pos = 0;
		bool strict;
		input::KeyData *evts;
		input::event_handler_fn handler;
	};

	struct MouseSequence {
		int id;
		int pos = 0;
		unsigned tolerance;
		input::MouseData *evts;
		input::event_handler_fn handler;
	};

	HHOOK keyboardHook = NULL;
	HHOOK mouseHook = NULL;

	HANDLE thread = NULL;
	DWORD threadId = 0;
	bool failure = false;

	std::list<input::key_handler_fn> keyHandlers;
	std::list<input::mouse_handler_fn> mouseHandlers;
	std::list<KeySequence> keyEventSeqs;
	std::list<MouseSequence> mouseEventSeqs;
	std::mutex keyHandlersMutex;
	std::mutex mouseHandlersMutex;
	std::mutex keyEventSeqsMutex;
	std::mutex mouseEventSeqsMutex;
	int seqCounter = 0;

	bool checkKeyHandlers(input::KeyData data) {
		if (keyHandlers.size() == 0) return false;

		bool stop = false;
		std::lock_guard<std::mutex> lock(keyHandlersMutex);
		for (auto& handler : keyHandlers) {
			stop = handler(data);
			if (stop) break;
		}
		return stop;
	}

	bool checkKeyEventHandlers(input::KeyData data) {
		if (keyEventSeqs.size() == 0) return false;

		bool stop = false;
		std::lock_guard<std::mutex> lock(keyEventSeqsMutex);
		for (auto& seq : keyEventSeqs) {
			input::KeyData& next = seq.evts[seq.pos];

			if (data.vkCode == next.vkCode && (!seq.strict ||
				(data.ctrl == next.ctrl && data.shift == next.shift && data.alt == next.alt))) {
				// increment pos on successful match
				_D("Key Seq " << seq.id << " matched: " << data.vkCode << std::endl);
				++seq.pos;

				if (seq.evts[seq.pos].vkCode == 0) {
					// complete sequence matched
					stop = seq.handler();
					seq.pos = 0;
					if (stop) break;
				}

			} else {
				// reset pos on a failed match
				seq.pos = 0;
			}

		}
		return stop;
	}

	bool checkMouseHandlers(input::MouseData data) {
		if (mouseHandlers.size() == 0) return false;

		bool stop = false;
		std::lock_guard<std::mutex> lock(mouseHandlersMutex);
		for (auto& handler : mouseHandlers) {
			stop = handler(data);
			if (stop) break;
		}
		return stop;
	}

	bool checkMouseEventHandlers(input::MouseData data) {
		if (mouseEventSeqs.size() == 0) return false;

		bool stop = false;
		std::lock_guard<std::mutex> lock(mouseEventSeqsMutex);
		for (auto& seq : mouseEventSeqs) {
			input::MouseData& next = seq.evts[seq.pos];
			int tol = (unsigned)seq.tolerance;

			if (next.code == data.code && next.x >= data.x - tol && next.x <= data.x + tol &&
				next.y >= data.y - tol && next.y <= data.y + tol) {
				// increment pos on successful match
				_D("Mouse Seq " << seq.id << " matched: " << data.code \
					<< ", " << data.x << ", " << data.y << std::endl);
				++seq.pos;

				if (seq.evts[seq.pos].code == 0) {
					// complete sequence matched
					stop = seq.handler();
					seq.pos = 0;
					if (stop) break;
				}

			} else {
				// reset pos on a failed match
				seq.pos = 0;
			}
		}
		return stop;
	}

	// callback function for keyboard hook
	LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
		bool stop = false;

		if (code == HC_ACTION) {
			LPKBDLLHOOKSTRUCT key = (LPKBDLLHOOKSTRUCT)lParam;

			if ((key->flags >> LLKHF_INJECTED) & 1) {
				// ignore injected events
			} else {
				// get current state of modifier keys
				SHORT ctrl = GetAsyncKeyState(VK_CONTROL) >> (sizeof(SHORT) - 1);
				SHORT shift = GetAsyncKeyState(VK_SHIFT) >> (sizeof(SHORT) - 1);
				SHORT alt = GetAsyncKeyState(VK_MENU) >> (sizeof(SHORT) - 1);

				short type = INPUT_TYPE_KEYUP;
				if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
					type = INPUT_TYPE_KEYDOWN;

				input::KeyData data = { key->vkCode, ctrl != 0, shift != 0, alt != 0, type };

				// sequences are only processed on key down
				if (type == INPUT_TYPE_KEYDOWN) {
					stop = checkKeyEventHandlers(data);
					if (stop) return 1;
				}

				stop = checkKeyHandlers(data);
				if (stop) return 1;
			}
		}

		return CallNextHookEx(NULL, code, wParam, lParam);
	}

	// callback function for mouse hook
	LRESULT CALLBACK lowLevelMouseProc(int code, WPARAM wParam, LPARAM lParam) {
		bool stop = false;

		if (code == HC_ACTION) {
			LPMSLLHOOKSTRUCT inf = (LPMSLLHOOKSTRUCT)lParam;

			if ((inf->flags >> LLMHF_INJECTED) & 1) {
				// ignore injected events
				std::cout << "inj " << wParam << std::endl;
			} else {
				input::MouseData data = { wParam, inf->pt.x, inf->pt.y, inf->mouseData };

				// skip mouse move events for sequences
				if (wParam != WM_MOUSEMOVE) {
					stop = checkMouseEventHandlers(data);
					if (stop) return 1;
				}

				stop = checkMouseHandlers(data);
				if (stop) return 1;
			}
		}

		return CallNextHookEx(NULL, code, wParam, lParam);
	}

	// the main function of the internal wininput thread
	DWORD WINAPI _main(LPVOID lpParam) {
		_D("WinInput thread started." << std::endl);
		keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, lowLevelKeyboardProc, NULL, 0);
		mouseHook = SetWindowsHookEx(WH_MOUSE_LL, lowLevelMouseProc, NULL, 0);

		BOOL bRet;
		MSG msg;
		while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
			if (bRet != -1) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		UnhookWindowsHookEx(keyboardHook);
		UnhookWindowsHookEx(mouseHook);
		keyboardHook = NULL;
		mouseHook = NULL;
		_D("WinInput thread stopped." << std::endl);

		return 0;
	}

	// create the wininput thread if it has not been done before
	bool setupThread() {
		if (failure) return false;
		if (thread != NULL) return true;

		_D("Creating WinInput thread." << std::endl);
		thread = CreateThread(NULL, 0, _main, NULL, 0, &threadId);

		if (thread == NULL) failure = true;
		return thread != NULL;
	}
}

namespace input {

	bool addKeyHandler(key_handler_fn fn) {
		bool res = setupThread();
		std::lock_guard<std::mutex> lock(keyHandlersMutex);
		keyHandlers.push_back(fn);
		return res;
	}

	bool addMouseHandler(mouse_handler_fn fn) {
		bool res = setupThread();
		std::lock_guard<std::mutex> lock(mouseHandlersMutex);
		mouseHandlers.push_back(fn);
		return res;
	}

	bool removeKeyHandler(key_handler_fn fn) {
		std::lock_guard<std::mutex> lock(keyHandlersMutex);
		for (auto it = keyHandlers.begin(); it != keyHandlers.end(); ++it) {
			if (*it == fn) {
				keyHandlers.erase(it);
				return true;
			}
		}
		return false;
	}

	bool removeMouseHandler(mouse_handler_fn fn) {
		std::lock_guard<std::mutex> lock(mouseHandlersMutex);
		for (auto it = mouseHandlers.begin(); it != mouseHandlers.end(); ++it) {
			if (*it == fn) {
				mouseHandlers.erase(it);
				return true;
			}
		}
		return false;
	}

	bool onKeyEvent(KeyData *data, bool strict, event_handler_fn fn, int *handlerId) {
		bool res = setupThread();
		int seqId = ++seqCounter;
		if (handlerId) *handlerId = seqId;
		KeySequence seq = { seqId, 0, strict, data, fn };

		std::lock_guard<std::mutex> lock(keyEventSeqsMutex);
		keyEventSeqs.push_back(seq);
		return res;
	}

	bool onMouseEvent(MouseData *data, unsigned tolerance, event_handler_fn fn, int *handlerId) {
		bool res = setupThread();
		int seqId = ++seqCounter;
		if (handlerId) *handlerId = seqId;
		MouseSequence seq = { seqId, 0, tolerance, data, fn };

		std::lock_guard<std::mutex> lock(mouseEventSeqsMutex);
		mouseEventSeqs.push_back(seq);
		return res;
	}

	bool removeKeyEventHandler(int handlerId) {
		std::lock_guard<std::mutex> lock(keyEventSeqsMutex);
		for (auto it = keyEventSeqs.begin(); it != keyEventSeqs.end(); ++it) {
			if (handlerId == it->id) {
				keyEventSeqs.erase(it);
				return true;
			}
		}
		return false;
	}

	bool removeMouseEventHandler(int handlerId) {
		std::lock_guard<std::mutex> lock(mouseEventSeqsMutex);
		for (auto it = mouseEventSeqs.begin(); it != mouseEventSeqs.end(); ++it) {
			if (handlerId == it->id) {
				mouseEventSeqs.erase(it);
				return true;
			}
		}
		return false;
	}

	void shutdown() {
		if (thread != NULL) {
			PostThreadMessage(threadId, WM_QUIT, NULL, NULL);
			WaitForSingleObject(thread, INFINITE);
			CloseHandle(thread);
			thread = NULL;
			threadId = 0;
		}

		failure = false;
		_D("WinInput shutdown complete." << std::endl);
	}
}