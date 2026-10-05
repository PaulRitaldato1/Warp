#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleObject.h>
#include <Debugging/Logging.h>
#include <Events/DelegateDefs.h>

class IWindow;
struct ImGuiInputTextCallbackData;

// The in game console. Backtick opens and closes it. Shows every log line and
// runs typed lines through the ConsoleRegistry, so output from commands and
// cvars is just logging. Up and down step through history, Tab completes names.
class WARP_API ConsoleWindow
{
public:
	ConsoleWindow();
	~ConsoleWindow();

	ConsoleWindow(const ConsoleWindow&)			   = delete;
	ConsoleWindow& operator=(const ConsoleWindow&) = delete;

	// Opening the console releases the mouse so the cursor can reach it.
	void SetWindow(IWindow* window)
	{
		m_window = window;
	}

	// Every frame inside the ImGui frame, open or not, since it also checks the toggle key.
	void Draw();

private:
	struct Line
	{
		LogLevel level;
		String text;
		bool bCommand; // the echo of a typed line
	};

	void OnLog(LogLevel level, const String& message);
	void OnClear(const ConsoleArgs& args);

	void AddLine(LogLevel level, String text, bool bCommand);
	void Submit(const String& line);

	int TextEditCallback(ImGuiInputTextCallbackData* data);
	void CompleteName(ImGuiInputTextCallbackData* data);
	void StepHistory(ImGuiInputTextCallbackData* data);

	IWindow* m_window		= nullptr;
	bool m_bOpen			= false;
	bool m_bFocusInput		= false;
	bool m_bScrollToBottom	= false;

	// Logs arrive from any thread, so every access to m_lines locks.
	Mutex m_linesMutex;
	Vector<Line> m_lines;

	Array<char, 256> m_input{};
	Vector<String> m_history; // oldest first
	int32 m_historyPos = -1;  // -1 is the line being typed

	URef<MemberFuncType<ConsoleWindow, LogLevel, const String&>> m_logDelegate;
	URef<MemberFuncType<ConsoleWindow, const ConsoleArgs&>> m_clearDelegate;
};
