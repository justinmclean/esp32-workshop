// Included by every workshop project. Gives a clear error instead of a wall
// of "undeclared identifier" messages when secrets.h has not been made yet.
#pragma once
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h not found. Copy secrets.example.h to secrets.h in the repo root and fill it in."
#endif
