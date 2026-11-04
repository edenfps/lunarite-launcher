// Log.h - extra logger entry points beyond WZCommon.h.
#pragma once
#include "WZCommon.h"

// Fills out[260] with "<exe dir>\WZLauncher.Log" ( path part) and makes it the active log file.
// Pass nullptr to (re)initialise the internal path only.
void WZLogInitPath(wchar_t* out);
// Active log file path ("" until WZLogInitPath / WZLogBanner / first WZLog).
const wchar_t* WZLogGetPath();
