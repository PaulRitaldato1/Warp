#include <UI/ConsoleWindow.h>

#include <Core/Console/ConsoleCommand.h>
#include <Core/Console/ConsoleRegistry.h>
#include <Rendering/Window/Window.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>

static ConsoleCommand CmdClear("clear", "Clear the console output");

// Old lines are dropped in chunks once the count passes the max, so the erase
// from the front is rare.
static constexpr size_t k_maxLines	= 2000;
static constexpr size_t k_dropLines = 500;

static String ToLower(const String& text)
{
	String lower = text;
	std::transform(lower.begin(), lower.end(), lower.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

static ImVec4 GetLineColor(LogLevel level, bool bCommand)
{
	if (bCommand)
	{
		return ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
	}

	switch (level)
	{
		case LogLevel::Verbose:
		case LogLevel::Debug:
			return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
		case LogLevel::Warning:
			return ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
		case LogLevel::Error:
			return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
		default:
			return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

ConsoleWindow::ConsoleWindow()
{
	m_logDelegate = std::make_unique<MemberFuncType<ConsoleWindow, LogLevel, const String&>>(this, &ConsoleWindow::OnLog);
	Logger::Get().OnLog().Subscribe(m_logDelegate.get());

	m_clearDelegate = std::make_unique<MemberFuncType<ConsoleWindow, const ConsoleArgs&>>(this, &ConsoleWindow::OnClear);
	CmdClear.Subscribe(m_clearDelegate.get());
}

ConsoleWindow::~ConsoleWindow()
{
	Logger::Get().OnLog().Unsubscribe(m_logDelegate.get());
	CmdClear.Unsubscribe(m_clearDelegate.get());
}

void ConsoleWindow::OnLog(LogLevel level, const String& message)
{
	// One entry per line keeps every entry the same height, which the clipper needs.
	size_t start = 0;
	while (start <= message.size())
	{
		const size_t end = message.find('\n', start);
		if (end == String::npos)
		{
			AddLine(level, message.substr(start), false);
			break;
		}
		AddLine(level, message.substr(start, end - start), false);
		start = end + 1;
	}
}

void ConsoleWindow::OnClear(const ConsoleArgs& /*args*/)
{
	std::lock_guard lock(m_linesMutex);
	m_lines.clear();
}

void ConsoleWindow::AddLine(LogLevel level, String text, bool bCommand)
{
	std::lock_guard lock(m_linesMutex);

	if (m_lines.size() >= k_maxLines)
	{
		m_lines.erase(m_lines.begin(), m_lines.begin() + k_dropLines);
	}
	m_lines.push_back({ level, std::move(text), bCommand });
}

void ConsoleWindow::Submit(const String& line)
{
	const size_t first = line.find_first_not_of(" \t");
	if (first == String::npos)
	{
		return;
	}
	const String trimmed = line.substr(first, line.find_last_not_of(" \t") - first + 1);

	AddLine(LogLevel::Info, "> " + trimmed, true);

	// Most recent at the end, without repeats, so up arrow gets the last distinct line.
	m_history.erase(std::remove(m_history.begin(), m_history.end(), trimmed), m_history.end());
	m_history.push_back(trimmed);
	m_historyPos = -1;

	// Output comes back through OnLog.
	ConsoleRegistry::Get().Execute(trimmed);
	m_bScrollToBottom = true;
}

void ConsoleWindow::Draw()
{
	// ImGui sees every key even while the game has the mouse, so this works in
	// both modes. Not repeated, so holding it does not flicker.
	if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
	{
		m_bOpen = !m_bOpen;
		if (m_bOpen)
		{
			m_bFocusInput	  = true;
			m_bScrollToBottom = true;
			if (m_window)
			{
				m_window->ReleaseMouse();
			}
		}
	}

	if (!m_bOpen)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(720.f, 360.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console", &m_bOpen))
	{
		ImGui::End();
		return;
	}

	// Output fills everything above the input line.
	const f32 footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("Output", ImVec2(0.f, -footerHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
	{
		std::lock_guard lock(m_linesMutex);

		// Only submits the lines in view, so a full history costs nothing.
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(m_lines.size()));
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const Line& line = m_lines[i];
				ImGui::PushStyleColor(ImGuiCol_Text, GetLineColor(line.level, line.bCommand));
				ImGui::TextUnformatted(line.text.c_str());
				ImGui::PopStyleColor();
			}
		}

		// Follows new lines only while already at the bottom, so scrolling up to
		// read is not yanked back down.
		if (m_bScrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}
		m_bScrollToBottom = false;
	}
	ImGui::EndChild();

	ImGui::Separator();

	// A captureless lambda converts to ImGui's callback pointer, and being inside
	// a member function it can reach the private callback.
	const ImGuiInputTextCallback callback = [](ImGuiInputTextCallbackData* data) -> int
	{ return static_cast<ConsoleWindow*>(data->UserData)->TextEditCallback(data); };

	const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll |
									  ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory |
									  ImGuiInputTextFlags_CallbackCharFilter;

	if (m_bFocusInput)
	{
		ImGui::SetKeyboardFocusHere();
		m_bFocusInput = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputText("##Input", m_input.data(), m_input.size(), flags, callback, this))
	{
		Submit(m_input.data());
		m_input[0] = '\0';

		// Enter drops focus, so take it back for the next line.
		m_bFocusInput = true;
	}

	ImGui::End();
}

int ConsoleWindow::TextEditCallback(ImGuiInputTextCallbackData* data)
{
	switch (data->EventFlag)
	{
		case ImGuiInputTextFlags_CallbackCharFilter:
		{
			// The toggle key would otherwise type itself into the box. 1 rejects the char.
			return data->EventChar == '`' || data->EventChar == '~' ? 1 : 0;
		}
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			CompleteName(data);
			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			StepHistory(data);
			break;
		}
		default:
		{
			break;
		}
	}
	return 0;
}

void ConsoleWindow::CompleteName(ImGuiInputTextCallbackData* data)
{
	// Only the first word is a name. Arguments have nothing to complete against.
	const String typed(data->Buf, data->CursorPos);
	if (typed.find(' ') != String::npos)
	{
		return;
	}

	const String prefix = ToLower(typed);
	Vector<const ConsoleObject*> matches;
	for (const ConsoleObject* object : ConsoleRegistry::Get().GetAll())
	{
		if (ToLower(object->GetName()).starts_with(prefix))
		{
			matches.push_back(object);
		}
	}

	if (matches.empty())
	{
		AddLine(LogLevel::Info, "No match for '" + typed + "'", false);
		return;
	}

	if (matches.size() == 1)
	{
		data->DeleteChars(0, data->CursorPos);
		data->InsertChars(0, (matches[0]->GetName() + " ").c_str());
		return;
	}

	// Several matches: fill in what they all share, then list them so the next
	// letters to type are visible.
	String common = matches[0]->GetName();
	for (const ConsoleObject* match : matches)
	{
		const String& name = match->GetName();
		size_t length	   = 0;
		while (length < common.size() && length < name.size() &&
			   std::tolower(static_cast<unsigned char>(common[length])) ==
				   std::tolower(static_cast<unsigned char>(name[length])))
		{
			++length;
		}
		common.resize(length);
	}

	data->DeleteChars(0, data->CursorPos);
	data->InsertChars(0, common.c_str());

	for (const ConsoleObject* match : matches)
	{
		AddLine(LogLevel::Info, "  " + match->GetName(), false);
	}
	m_bScrollToBottom = true;
}

void ConsoleWindow::StepHistory(ImGuiInputTextCallbackData* data)
{
	if (m_history.empty())
	{
		return;
	}

	const int32 previous = m_historyPos;
	const int32 count	 = static_cast<int32>(m_history.size());

	if (data->EventKey == ImGuiKey_UpArrow)
	{
		if (m_historyPos == -1)
		{
			m_historyPos = count - 1;
		}
		else if (m_historyPos > 0)
		{
			--m_historyPos;
		}
	}
	else if (data->EventKey == ImGuiKey_DownArrow)
	{
		// Past the newest entry goes back to an empty line.
		if (m_historyPos != -1 && ++m_historyPos >= count)
		{
			m_historyPos = -1;
		}
	}

	if (previous != m_historyPos)
	{
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, m_historyPos >= 0 ? m_history[m_historyPos].c_str() : "");
	}
}
