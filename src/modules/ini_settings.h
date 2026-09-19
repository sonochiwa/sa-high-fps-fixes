#pragma once

#include "modules/prelude.h"

namespace hff {

const std::string& DefaultIniText();
bool CreateDefaultIniIfMissing();

struct IniCompletionResult {
    size_t added{};
    bool complete{true};
};
bool RefreshIniVersionHeader();
IniCompletionResult CompleteIniWithMissingDefaults();
void MigrateIniLayout();
void RegisterConfigKey(const char* section, const char* key);
void AddConfigWarning(const char* section, const char* key, const char* reason);
bool ReadSetting(const char* section, const char* key, bool defaultValue);
int ReadNumber(const char* section, const char* key, int defaultValue);
bool IsKnownConfigKey(const char* section, const char* key);
void RegisterConditionalConfigKeys();
bool IsRetiredConfigKey(const char* section, const char* key);
void ValidateUnknownConfigKeys();
void ReportConfigWarnings();

} // namespace hff
