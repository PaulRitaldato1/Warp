#pragma once

#include <unordered_map>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <stack>
#include <utility>
#include <queue>
#include <mutex>

#if defined(_WIN64)
#include <wrl/client.h>

template <typename T>
using ComRef = Microsoft::WRL::ComPtr<T>;
#endif

//These are all just set to their std equivalents. The point of this is so that I can swap out certain types for custom ones if I need to without issue.
//ex: Custom unordered_map for faster lookups
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using uint = unsigned int;
using byte = unsigned char;

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using f32 = float;
using f64 = double;

using String = std::string;
using WString = std::wstring;

template <typename T, typename J>
using Pair = std::pair<T, J>;

template <typename T, size_t J>
using Array = std::array<T, J>;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
using Ref = std::shared_ptr<T>;

template <typename T>
using URef = std::unique_ptr<T>;

template <typename T, typename J>
using HashMap = std::unordered_map<T, J>;

template <typename T, typename J>
using Map = std::map<T, J>;

template <typename T>
using Stack = std::stack<T>;

template <typename T>
using Queue = std::queue<T>;

using Mutex = std::mutex;

#if defined(_WIN64)
inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception();
	}
}
#endif

inline WString StringToWString(const char* string)
{
	String tmp(string);
	return WString(tmp.begin(), tmp.end());
}

inline WString StringToWString(const String& string)
{
	return WString(string.begin(), string.end());
}

template<class T>
inline T AlignOffset(const T& uOffset, const T& uAlign) { return ((uOffset + (uAlign - 1)) & ~(uAlign - 1)); }

//Platform
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
	#define WARP_WINDOWS 1
	#define WARP_BUILD_DX12 1
	#define WARP_BUILD_VK 1
#elif defined(__linux__) || defined(__gnu_linux__)
	#define WARP_LINUX 1
	#define WARP_BUILD_VK 1
#elif defined(__APPLE__)
	#define WARP_APPLE 1
	#define WARP_BUILD_METAL 1
#endif


#ifdef WARP_EXPORT

//export
#ifdef  _MSC_VER
	#define WARP_API __declspec(dllexport)
#else
	#define WARP_API __attribute__((visibility("default")))
#endif

//import
#ifdef _MSC_VER
	#define WARP_API __declspec(dllimport)
#else
	#define WARP_API
#endif
#endif