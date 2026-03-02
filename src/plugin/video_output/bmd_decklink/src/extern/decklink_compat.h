#pragma once
#include <string>
#if defined(__APPLE__)
  #include <CoreFoundation/CoreFoundation.h>
  inline std::string decklink_string_to_std(CFStringRef cfstr) {
      if (!cfstr) return "";
      char buf[512];
      if (CFStringGetCString(cfstr, buf, sizeof(buf), kCFStringEncodingUTF8))
          return std::string(buf);
      return "";
  }
  inline void decklink_free_string(CFStringRef cfstr) { if (cfstr) CFRelease(cfstr); }
  #define DECKLINK_STR CFStringRef
  #define BOOL bool
#elif defined(_WIN32)

#include <comdef.h>
#include <comutil.h>
#include <Shlwapi.h>
#include <string>
#include <functional>
#include <stdint.h>

inline std::string decklink_string_to_std(BSTR dl_str) { 
	int wlen = ::SysStringLen(dl_str);
	int mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, NULL, 0, NULL, NULL);

	std::string ret_str(mblen, '\0');
	mblen = ::WideCharToMultiByte(CP_ACP, 0, (wchar_t*)dl_str, wlen, &ret_str[0], mblen, NULL, NULL);

	return ret_str;
}
inline void decklink_free_string(BSTR str) { ::SysFreeString(str); }

#define DECKLINK_STR BSTR

#elif defined(__linux__)

  #define DECKLINK_STR const char*
  inline std::string decklink_string_to_std(DECKLINK_STR str) { return str ? str : ""; }
  inline void decklink_free_string(DECKLINK_STR) { /* Linux SDK manages string lifetime */ }
  #define BOOL bool

#endif
