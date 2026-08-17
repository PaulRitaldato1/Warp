#pragma once

#include <Common/CommonTypes.h>

// Why an asset failed to load. The code is for branching, the message for logging.
enum class LoadErrorCode : u8
{
	FileNotFound,	   // path does not exist, or could not be opened
	ParseFailed,	   // file exists but its contents could not be decoded
	UnsupportedFormat, // decoded fine, but the format is one we cannot use
};

struct LoadError
{
	LoadErrorCode code = LoadErrorCode::ParseFailed;
	String		  message;
};

constexpr const char* ToString(LoadErrorCode code)
{
	switch (code)
	{
		case LoadErrorCode::FileNotFound:	   return "FileNotFound";
		case LoadErrorCode::ParseFailed:	   return "ParseFailed";
		case LoadErrorCode::UnsupportedFormat: return "UnsupportedFormat";
	}
	return "Unknown";
}

// Terse construction at return sites: `return MakeLoadError(code, msg);`
inline std::unexpected<LoadError> MakeLoadError(LoadErrorCode code, String message)
{
	return std::unexpected(LoadError{ code, std::move(message) });
}
