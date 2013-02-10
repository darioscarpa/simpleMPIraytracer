#include "console.h"

#include <windows.h>
#include <stdio.h>

bool Console::enabled = false;

void Console::open() {
    AllocConsole();					  // Create Console Window
    freopen("CONIN$", "rb", stdin);   // reopen stdin handle as console window input
	freopen("CONOUT$", "wb", stdout); // reopen stout handle as console window output
    freopen("CONOUT$", "wb", stderr); // reopen stderr handle as console window output
}

void Console::close() {
	FreeConsole();  // Free Console Window
}

