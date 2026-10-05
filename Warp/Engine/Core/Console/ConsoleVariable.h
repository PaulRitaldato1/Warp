#pragma once

#include <Common/CommonTypes.h>
#include <Core/Console/ConsoleObject.h>
#include <Debugging/Logging.h>
#include <Events/DelegateDefs.h>

#include <charconv>
#include <format>
#include <type_traits>

enum class CvarFlags : u8
{
	None = 0,

	// Only read at startup, like r.GraphicsAPI. Set it in Engine.ini; the console
	// refuses changes once startup is finished.
	Startup = 1 << 0,
};

inline bool HasFlag(CvarFlags flags, CvarFlags flag)
{
	return (static_cast<u8>(flags) & static_cast<u8>(flag)) != 0;
}

// Everything goes through strings here, so the console never needs to know a
// variable's type.
class ConsoleVariableBase : public ConsoleObject
{
public:
	ConsoleVariableBase(const char* name, const char* help, CvarFlags flags)
		: ConsoleObject(name, help)
		, m_flags(flags)
	{
	}

	// False if text does not parse as this variable's type. The value is left as is.
	virtual bool SetFromString(const String& text) = 0;
	virtual String ToString() const				   = 0;

	// "r.GPUCulling" prints the value, "r.GPUCulling 0" sets it.
	bool Execute(const ConsoleArgs& args) override
	{
		if (args.empty())
		{
			LOG_INFO("{} = {}", GetName(), ToString());
			return true;
		}

		// Refused rather than set, so the value always matches what the engine is running with.
		if (HasFlag(m_flags, CvarFlags::Startup) && ConsoleRegistry::Get().IsStartupFinished())
		{
			LOG_WARNING("{} is only read at startup. Set it in Engine.ini and restart", GetName());
			return false;
		}

		// Joined back so a string value can be typed without quotes.
		String text = args[0];
		for (size_t i = 1; i < args.size(); ++i)
		{
			text += ' ';
			text += args[i];
		}

		if (!SetFromString(text))
		{
			LOG_WARNING("'{}' is not a valid value for {}", text, GetName());
			return false;
		}

		LOG_INFO("{} = {}", GetName(), ToString());
		return true;
	}

	CvarFlags GetFlags() const
	{
		return m_flags;
	}

private:
	CvarFlags m_flags;
};

// What the code using it sees. Declared as a global next to that code:
//   static Cvar<bool> CvarGPUCulling("r.GPUCulling", true, "Cull instances on the GPU");
// Get is a plain member read: no lookup, no type check, cheap in a hot loop.
template <typename T>
class Cvar : public ConsoleVariableBase
{
	static_assert(std::is_same_v<T, bool> || std::is_same_v<T, int32> || std::is_same_v<T, f32> ||
					  std::is_same_v<T, String>,
				  "Cvar supports bool, int32, f32 and String");

	// Numbers and bools come back by value, strings by reference so a read in a
	// hot loop never copies one.
	using GetType = std::conditional_t<std::is_arithmetic_v<T>, T, const T&>;

public:
	Cvar(const char* name, T defaultValue, const char* help, CvarFlags flags = CvarFlags::None)
		: ConsoleVariableBase(name, help, flags)
		, m_value(std::move(defaultValue))
	{
	}

	GetType Get() const
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

		m_value = std::move(value);
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
		else if constexpr (std::is_same_v<T, String>)
		{
			// Any text is a valid string, spaces included.
			Set(text);
			return true;
		}
		else
		{
			// int32 and f32. from_chars has no exceptions and no locale, so "0.5"
			// parses the same on every machine, and requiring it to consume the
			// whole string rejects "12abc", which atoi would read as 12.
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
		else if constexpr (std::is_same_v<T, String>)
		{
			return m_value;
		}
		else
		{
			// Shortest text that reads back as the same value: 0.5 rather than
			// to_string's 0.500000.
			return std::format("{}", m_value);
		}
	}

private:
	T m_value;
	EventManager<T> m_onChanged;
};
