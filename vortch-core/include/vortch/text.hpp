#pragma once

// UTF-8 Everywhere policy: every std::string in vortch-core is UTF-8. Convert
// ONLY at OS boundaries (Win32 wide APIs, filesystem paths on Windows). Do NOT
// use char8_t / std::u8string.

#include <string>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace vortch {

#ifdef _WIN32

inline std::wstring utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<size_t>(n), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

inline std::string wideToUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                       nullptr, 0, nullptr, nullptr);
  std::string s(static_cast<size_t>(n), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        s.data(), n, nullptr, nullptr);
  return s;
}

// Build a filesystem::path from a UTF-8 string (Windows path value is wchar_t).
inline std::filesystem::path utf8ToPath(const std::string& s) {
  return std::filesystem::path(utf8ToWide(s));
}
inline std::string pathToUtf8(const std::filesystem::path& p) {
  return wideToUtf8(p.wstring());
}

#else  // POSIX: paths are already UTF-8 byte sequences.

inline std::filesystem::path utf8ToPath(const std::string& s) {
  return std::filesystem::path(s);
}
inline std::string pathToUtf8(const std::filesystem::path& p) {
  return p.string();
}

#endif

} // namespace vortch
