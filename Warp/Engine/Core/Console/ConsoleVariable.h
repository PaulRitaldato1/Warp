#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleRegistry.h>
#include <Events/DelegateDefs.h>

#include <charconv>
#include <type_traits>

// What the registry and the console see. Everything goes through strings, so
// nothing here needs to know a variable's type.
class ConsoleVariableBase
{
public:
	// Registers on construction and unregisters on destruction. Cvars are
	// globals, so this runs before main and after it returns.
	ConsoleVariableBase(const char* name, const char* help)
		: m_name(name)
		, m_help(help)
	{
		ConsoleRegistry::Get().Register(this);
	}

	// Safe at exit: the registry is created by the first registration, so as a
	// static it is destroyed after every variable constructed after it.
	virtual ~ConsoleVariableBase()
	{
		ConsoleRegistry::Get().Unregister(this);
	}

	ConsoleVariableBase(const ConsoleVariableBase&)			   = delete;
	ConsoleVariableBase& operator=(const ConsoleVariableBase&) = delete;

	// False if text does not parse as this variable's type. The value is left as is.
	virtual bool SetFromString(const String& text) = 0;
	virtual String ToString() const				   = 0;

	const String& GetName() const
	{
		return m_name;
	}

	const String& GetHelp() const
	{
		return m_help;
	}

private:
	String m_name;
	String m_help;
};

// What the code using it sees. Declared as a global next to that code:
//   static Cvar<bool> CvarGPUCulling("r.GPUCulling", true, "Cull instances on the GPU");
// Get is a plain member read: no lookup, no type check, cheap in a hot loop.
template <typename T>
class Cvar : public ConsoleVariableBase
{
	static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int32>, "Cvar supports bool and int32 so far");

public:
	Cvar(const char* name, T defaultValue, const char* help)
		: ConsoleVariableBase(name, help)
		, m_value(defaultValue)
	{
	}

	T Get() const
	{
		return m_value;
	}

	// Broadcasts only on an actual change, so setting a value it already has
	// never triggers work like a swap chain rebuild.
	void Set(T value)
	{
		if (value == m_value)
		{
			return;
		}

		m_value = value;
		m_onChanged.Broadcast(m_value);
	}

	// Called with the new value after every change, from code or the console.
	// The subscriber owns the delegate and must unsubscribe before destroying it,
	// since a cvar outlives everything that listens to it.
	void SubscribeToChanged(DelegateBase<T>* delegate)
	{
		m_onChanged.Subscribe(delegate);
	}

	void UnsubscribeFromChanged(DelegateBase<T>* delegate)
	{
		m_onChanged.Unsubscribe(delegate);
	}

	bool SetFromString(const String& text) override
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			// 1 and 0 to match Engine.ini, true and false for readability.
			if (text == "1" || text == "true" || text == "True" || text == "TRUE")
			{
				Set(true);
				return true;
			}
			if (text == "0" || text == "false" || text == "False" || text == "FALSE")
			{
				Set(false);
				return true;
			}
			return false;
		}
		else
		{
			// from_chars has no exceptions and no locale. Requiring it to consume
			// the whole string rejects "12abc", which atoi would read as 12.
			T parsed			   = 0;
			const char* end		   = text.data() + text.size();
			const auto [ptr, error] = std::from_chars(text.data(), end, parsed);
			if (error != std::errc{} || ptr != end)
			{
				return false;
			}
			Set(parsed);
			return true;
		}
	}

	String ToString() const override
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return m_value ? "true" : "false";
		}
		else
		{
			return std::to_string(m_value);
		}
	}

private:
	T m_value;
	EventManager<T> m_onChanged;
};
