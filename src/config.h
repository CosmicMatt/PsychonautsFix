#ifndef CONFIG_H
#define CONFIG_H

#include "xinput_translator.h"
#include "display_patcher.h"

struct ModConfig {
    ControllerConfig controller;
    DisplayConfig display;
};

class Config {
public:
    static ModConfig Load(const wchar_t* iniPath);
};

#endif // CONFIG_H
