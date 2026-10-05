#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleObject.h>
#include <Debugging/Logging.h>
#include <Events/DelegateDefs.h>

// A named event the console can fire. Declared as a global, like a Cvar:
//   static ConsoleCommand CmdHelp("help", "List commands", [](const ConsoleArgs& args) { ... });
//
// A handler that needs an object, like a system, subscribes when that object
// exists and unsubscribes before it goes away:
//   static ConsoleCommand CmdRespawn("game.Respawn", "Respawn the player");
//   CmdRespawn.Subscribe(m_respawnDelegate.get());
class ConsoleCommand : public ConsoleObject
{
public:
	ConsoleCommand(const char* name, const char* help)
		: ConsoleObject(name, help)
	{
	}

	// For handlers that need no object, so the command works from before main.
	ConsoleCommand(const char* name, const char* help, std::function<void(const ConsoleArgs&)> handler)
		: ConsoleObject(name, help)
		, m_handler(std::make_unique<FunctionDelegate<const ConsoleArgs&>>(std::move(handler)))
	{
		m_onExecute.Subscribe(m_handler.get());
	}

	void Subscribe(DelegateBase<const ConsoleArgs&>* delegate)
	{
		m_onExecute.Subscribe(delegate);
	}

	void Unsubscribe(DelegateBase<const ConsoleArgs&>* delegate)
	{
		m_onExecute.Unsubscribe(delegate);
	}

	bool Execute(const ConsoleArgs& args) override
	{
		// Declared but nothing listening, e.g. the system that handles it is not running.
		if (!m_onExecute.HasSubscribers())
		{
			LOG_WARNING("{} has no handler bound", GetName());
			return false;
		}

		m_onExecute.Broadcast(args);
		return true;
	}

private:
	EventManager<const ConsoleArgs&> m_onExecute;
	URef<FunctionDelegate<const ConsoleArgs&>> m_handler;
};
