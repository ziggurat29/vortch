#include "platform.hpp"

#include <windows.h>

namespace vortch {

static const char* kRunKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char* kValue  = "vortch";

bool installAutostart(const std::string& command) {
  HKEY key;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    return false;
  const LONG r = RegSetValueExA(key, kValue, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>(command.size() + 1));
  RegCloseKey(key);
  return r == ERROR_SUCCESS;
}

bool uninstallAutostart() {
  HKEY key;
  if (RegOpenKeyExA(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
    return false;
  const LONG r = RegDeleteValueA(key, kValue);
  RegCloseKey(key);
  return r == ERROR_SUCCESS;
}

} // namespace vortch
