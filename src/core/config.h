#pragma once

#include <string>

namespace hff {

// Points the settings at HighFpsFixes.ini. Settings can be read from then on,
// so `log` is read before anything else.
void SetConfigPath(const std::string& path);
// Creates the INI from the embedded default when it is missing, migrates keys
// of older releases, adds missing keys and refreshes the version header.
void PrepareConfig();
bool ReadSetting(const char* section, const char* key, bool defaultValue);
int ReadNumber(const char* section, const char* key, int defaultValue);
// Marks a key as known without reading it.
void RegisterConfigKey(const char* section, const char* key);
void AddConfigWarning(const char* section, const char* key, const char* reason);
// Logs unknown keys and every warning collected while reading.
void ReportConfigWarnings();

} // namespace hff
