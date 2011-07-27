#pragma once

// Add common controls 6.0 DLL to the manifest
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define STRICT							// Use strict declarations for Windows types

// Windows header
#include <windows.h>
#include <commctrl.h>
#include <psapi.h>

// Namespace usings
using namespace std;
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cmath>
#include <sstream>
#include <fstream>


#include "DXUT.h"
#include "SDKmisc.h"

// GDI+
#include <gdiplus.h>

// XNA Math and collision
#include <xnamath.h>
#include "xnaCollision.h"

// Some utility functions
#include "Utility.h"

// Static Lib Imports
#pragma comment(lib, "dxerr.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3D9.lib")
#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "D3DX10.lib")
#pragma comment(lib, "D3DX11.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#pragma comment(lib, "comsuppwd.lib")
#pragma comment(lib, "d3dx9d.lib")
#pragma comment(lib, "gwend_static.lib")
#pragma comment(lib, "tinyxmld.lib")
#pragma comment(lib, "assimpd.lib")
#else
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "gwen_static.lib")
#pragma comment(lib, "tinyxml.lib")
#pragma comment(lib, "assimp.lib")
#endif

// Warning disables
#pragma warning(disable: 4324) // Padding added from aligned members