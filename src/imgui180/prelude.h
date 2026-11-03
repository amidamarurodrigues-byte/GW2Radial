#pragma once

// Dear ImGui 1.80 (the version bundled with Nexus) is compiled into this DLL inside the namespace imgui180, so that it can
// coexist with the 1.91 copy the addon uses for its own overlay. Nexus' options panel lives in a 1.80 context, and calling
// 1.91 code on it would corrupt memory.
//
// Every system header ImGui includes is pulled in here, outside of the namespace, so the includes inside it do nothing.
#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
