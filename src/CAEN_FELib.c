/******************************************************************************
*
*	CAEN SpA - Software Division
*	Via Vetraia, 11 - 55049 - Viareggio ITALY
*	+39 0594 388 398 - www.caen.it
*
*******************************************************************************
*
*	Copyright (C) 2020-2023 CAEN SpA
*
*	This file is part of the CAEN FE Library.
*
*	The CAEN FE Library is free software; you can redistribute it and/or
*	modify it under the terms of the GNU Lesser General Public
*	License as published by the Free Software Foundation; either
*	version 3 of the License, or (at your option) any later version.
*
*	The CAEN FE Library is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
*	Lesser General Public License for more details.
*
*	You should have received a copy of the GNU Lesser General Public
*	License along with the CAEN FE Library; if not, see
*	https://www.gnu.org/licenses/.
*
*	SPDX-License-Identifier: LGPL-3.0-or-later
*
***************************************************************************//*!
*
*	\file		CAEN_FELib.c
*	\brief		Main source file
*	\author		Giovanni Cerretani, Matteo Fusco, Alberto Lucchesi
*
******************************************************************************/

#include "CAEN_FELib.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h> // getcwd
#include <Windows.h> // DisableThreadLibraryCalls (not working including libloaderapi.h before Windows.h)
#include "dirent_windows.h"
#else
#include <dirent.h>
#include <dlfcn.h> // dlopen, dlclose, dlsym, ...
#include <unistd.h> // getcwd
#endif

#include "definitions.h"

#define MAX_NUM_CONNECTION			128		// max number of devices that can be opened at the same time
#define MAX_NUM_LIBRARY				8		// max number of libraries (internal use, no specific size requirements)
#define HANDLE_PREFIX				UINT64_C(0xcae0)

// _Thread_local is C11 with thread support
#if (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_THREADS__)
#define THREAD_LOCAL				_Thread_local
#elif defined(_WIN32)
#define THREAD_LOCAL				__declspec(thread)
#elif defined(__GNUC__)
#define THREAD_LOCAL				__thread
#else
#error unsupported compiler
#endif

// _Static_assert is C11
#if (__STDC_VERSION__ >= 201112L)
#define STATIC_ASSERT(COND, MSG)	_Static_assert(COND, CAEN_FELIB_STR(MSG))
#else
#define STATIC_ASSERT(COND, MSG)	typedef char _static_assertion_##MSG[(COND) ? 1 : -1]
#endif

#define ARRAY_SIZE(x)				(sizeof(x)/sizeof((x)[0]))

static struct connection_descr* connectionDescr[MAX_NUM_CONNECTION];
static struct library_descr* libDescr[MAX_NUM_LIBRARY];
static THREAD_LOCAL char lastError[1024];

STATIC_ASSERT(ARRAY_SIZE(connectionDescr) <= UINT16_MAX, invalid_connection_size);		// connection index must be stored in 16 bits
STATIC_ASSERT(ARRAY_SIZE(connectionDescr) < UINT_FAST16_MAX, invalid_connection_type);	// UINT_FAST16_MAX index is reserved for invalid connection handle
STATIC_ASSERT(ARRAY_SIZE(libDescr) < UINT_FAST8_MAX, invalid_lib_type);					// UINT_FAST8_MAX index is reserved for invalid library handle
STATIC_ASSERT(ARRAY_SIZE(libDescr) <= ARRAY_SIZE(connectionDescr), invalid_descr_size);	// at most there can be a library for each connection
STATIC_ASSERT(ARRAY_SIZE(CAEN_FELIB_VERSION_STRING) <= 16, invalid_version_size);		// required by CAEN_FELib_GetLibVersion

// The format of loaded library filenames depends on the filesystem
#if defined(_WIN32)
#define CAEN_IMPL_DLL_PREFIX		"CAEN_"
#ifdef _DEBUG
#define CAEN_IMPL_DLL_SUFFIX		"LibD.dll"
#else
#define CAEN_IMPL_DLL_SUFFIX		"Lib.dll"
#endif
#elif defined(__APPLE__)
#define CAEN_IMPL_DLL_PREFIX		"libCAEN_"
#define CAEN_IMPL_DLL_SUFFIX		".dylib"
#else
#define CAEN_IMPL_DLL_PREFIX		"libCAEN_"
#define CAEN_IMPL_DLL_SUFFIX		".so"
#endif

// The format of loaded libraries API is fixed
#define CAEN_IMPL_API_PREFIX		"CAEN%s_"

static bool _allocateConnectionDescr(uint_fast16_t i) {
	if (i >= ARRAY_SIZE(connectionDescr))
		return false;
	struct connection_descr* descr = malloc(sizeof(*connectionDescr[i]));
	if (descr == NULL)
		return false;
	descr->arg[0] = '\0';
	descr->lHandle = UINT_FAST8_MAX;
	connectionDescr[i] = descr;
	return true;
}

static bool _resetConnectionDescr(uint_fast16_t i) {
	if (i >= ARRAY_SIZE(connectionDescr))
		return false;
	free(connectionDescr[i]);
	connectionDescr[i] = NULL;
	return true;
}

static bool _allocateLibDescr(uint_fast8_t i) {
	if (i >= ARRAY_SIZE(libDescr))
		return false;
	struct library_descr* descr = malloc(sizeof(*libDescr[i]));
	if (descr == NULL)
		return false;
	descr->APIVersion = LibraryAPIUnknown;
	descr->nRef = 0;
	descr->dlHandle = NULL;
	descr->GetLibInfo = NULL;
	descr->GetLibVersion = NULL;
	descr->GetLastError = NULL;
	descr->DevicesDiscovery = NULL;
	descr->Open = NULL;
	descr->Close = NULL;
	descr->GetDeviceTree = NULL;
	descr->GetChildHandles = NULL;
	descr->GetHandle = NULL;
	descr->GetParentHandle = NULL;
	descr->GetPath = NULL;
	descr->GetNodeProperties = NULL;
	descr->GetValue = NULL;
	descr->SetValue = NULL;
	descr->SendCommand = NULL;
	descr->GetUserRegister = NULL;
	descr->SetUserRegister = NULL;
	descr->SetReadDataFormat = NULL;
	descr->ReadDataV = NULL;
	descr->HasData = NULL;
	descr->name[0] = '\0';
	libDescr[i] = descr;
	return true;
}

static bool _resetLibDescr(uint_fast8_t i) {
	if (i >= ARRAY_SIZE(libDescr))
		return false;
	free(libDescr[i]);
	libDescr[i] = NULL;
	return true;
}

static size_t _countOccurrences(const char* name, char v) {
	size_t i; // https://stackoverflow.com/a/4235884/3287591
	for (i = 0; name[i] != '\0'; name[i] == v ? ++i : (size_t)++name);
	return i;
}

static bool _validateLibraryName(const char* name) {
	return _countOccurrences(name, '_') == 1;
}

// Given a string starting with "CAEN_XXXLib" returns XXX
// Note that the input string is changed ('L' is set to '\0')
static char* _getLibraryHWName(char* name) {
	const char prefix[] = CAEN_IMPL_DLL_PREFIX;
	const char suffix[] = CAEN_IMPL_DLL_SUFFIX;
	const size_t prefixSize = ARRAY_SIZE(prefix) - 1;
	if (strncmp(name, prefix, prefixSize) != 0)
		return NULL;
	name += prefixSize;
	char* const suffixPtr = strstr(name, suffix);
	if (suffixPtr == NULL)
		return NULL;
	*suffixPtr = '\0';
	return name;
}

static void _loadLibraryError(char* message, size_t message_size) {
#ifdef _WIN32
	// get the error message, if any
	const DWORD id = GetLastError();
	if (id == 0) {
		snprintf(message, message_size, "no error recorded");
		return;
	}

	// convert error to string
	LPSTR msg = NULL;
	const DWORD size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		id,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msg,
		0,
		NULL
	);

	if (size == 0) {
		snprintf(message, message_size, "error %"PRIuFAST32": FormatMessageA failed", (uint_fast32_t)id);
	} else {
		snprintf(message, message_size, "error %"PRIuFAST32": %s", (uint_fast32_t)id, msg);
	}

	LocalFree(msg);
#else
	char* msg = dlerror();

	if (msg == NULL) {
		snprintf(message, message_size, "error: no error found");
	} else {
		snprintf(message, message_size, "error: %s", msg);
	}
#endif
}

static dlHandle_t _loadLibrary(const char* libFileName) {
#ifdef _WIN32
	return LoadLibraryA((LPCSTR)libFileName);
#else
	dlerror(); // clear any existing error
	/*
	 * Adding RTLD_GLOBAL could be an alternative fix to GCC 67791 bug,
	 * currently fixed with "-Wl,--push-state,--no-as-needed,-lpthread,--pop-state"
	 * passed to the linker.
	 * See https://stackoverflow.com/a/51253734/3287591
	 */
	return dlopen(libFileName, RTLD_LAZY);
#endif
}

static bool _closeLibrary(dlHandle_t handle) {
#ifdef _WIN32
	return FreeLibrary(handle);
#else
	dlerror(); // clear any existing error
	/*
	 * dlclose() operation is not required to remove structures from an address space.
	 * Library function marked with __attribute__((destructor)) may not be called immediately.
	 * See https://pubs.opengroup.org/onlinepubs/007904975/functions/dlclose.html
	 */
	return dlclose(handle) == 0;
#endif
}

static bool _closeLibraryAndResetDevDescrIfLast(uint_fast8_t i) {
	bool ret = true;
	if (libDescr[i]->nRef == 0) {
		ret &= _closeLibrary(libDescr[i]->dlHandle);
		ret &= _resetLibDescr(i);
	}
	return ret;
}

static dlSymbol_t _getFunction(dlHandle_t dlHandle, const char* apiName) {
#ifdef _WIN32
	return GetProcAddress(dlHandle, apiName);
#else
	dlerror(); // clear any existing error
	return dlsym(dlHandle, apiName);
#endif
}

// set library name lowercase, with first character uppercase (e.g. "Dig2")
static void _adjustLibraryNameCase(char* name, size_t nameSize) {
	if (nameSize == 0)
		return;
	name[0] = (char)toupper(name[0]);
	for (size_t i = 1; i < nameSize; ++i)
		name[i] = (char)tolower(name[i]);
}

/*
 * URI management inspired from libwww 5.4.2 (HTParse.c)
 * License: W3C Software Notice and License
 * https://www.w3.org/Library/
 * Copyright 1995 - 2003 W3C (MIT, ERCIM, Keio, Beihang)
 * Copyright 1995 CERN
 */
struct hturi {
	char* scheme;
	char* host;
	char* absolute;
	char* relative;
	char* fragment;
};

static struct hturi _scanURI(char* const name) {
	char* p;
	char* afterScheme = name;
	struct hturi parts = {
		.scheme = NULL,
		.host = NULL,
		.absolute = NULL,
		.relative = NULL,
		.fragment = NULL
	};
	// look for fragment identifier
	if ((p = strchr(name, '#')) != NULL) {
		*p++ = '\0';
		parts.fragment = p;
	}
	// remove anything beyond a possible space
	// CAEN: libwww limits the search to simple spaces with strchr(name, ' ')
	if ((p = strpbrk(name, "\n\t\r\f\v ")) != NULL)
		*p = '\0';
	p = name;
	bool scan = true;
	do {
		// CAEN: libwww look for whitespaces removed as bugged and pretty useless
		switch (*p) {
		case ':':
			*p = '\0';
			parts.scheme = afterScheme;		// scheme has been specified
			afterScheme = p + 1;
			// CAEN: libwww check for possible "URL:" prefix removed as no longer recommended
			scan = false;
			break;
		case '/':
		case '?':							// CAEN: case '#' removed as impossible (any '#' has been set to '\0')
		case '\0':							// CAEN: new case to stop loop
			scan = false;
			break;
		default:
			++p;
			break;
		}
	} while (scan);
	p = afterScheme;
	if (*p == '/') {
		if (p[1] == '/') {
			parts.host = p + 2;				// host has been specified
			*p = '\0';						// terminate access
			p = strchr(parts.host, '/');	// look for end of host name if any
			if (p != NULL) {
				*p = '\0';					// terminate host
				parts.absolute = p + 1;		// root has been found
			}
		} else {
			parts.absolute = p + 1;			// root found but no host
		}
	} else {
		parts.relative = (*afterScheme) ? afterScheme : NULL; // zero for ""
	}
	return parts;
}

/*
 * Handles have this format:
 * 0xCAE0LLLLHHHHHHHH
 * where:
 * - CAE0 is a custom prefix
 * - LLLL is the cHandle (16 bits)
 * - HHHHHHHH the rHandle (32 bits)
 */

static bool _isValid(uint_fast16_t cHandle) {
	return (cHandle < ARRAY_SIZE(connectionDescr)) && (connectionDescr[cHandle] != NULL);
}

// connection handle
static uint_fast16_t _cHandle(uint64_t handle) {
	return ((handle >> 48) == HANDLE_PREFIX) ? (uint_fast16_t)((handle >> 32) & UINT64_C(0xffff)) : UINT_FAST16_MAX;
}

// remote handle
static uint32_t _rHandle(uint64_t handle) {
	return (uint32_t)handle;
}

// user handle
static uint64_t _handle(uint_fast16_t cHandle, uint32_t rHandle) {
	return (HANDLE_PREFIX << 48) | ((uint64_t)cHandle << 32) | (uint64_t)rHandle;
}

static struct library_descr* _getLibDescr(uint64_t handle) {
	const uint_fast16_t cHandle = _cHandle(handle);
	return _isValid(cHandle) ? libDescr[connectionDescr[cHandle]->lHandle] : NULL;
}

static int _loadAPIv0(struct library_descr* descr) {
	char apiName[64];
	const size_t apiNameSize = ARRAY_SIZE(apiName);
	const dlHandle_t dlHandle = descr->dlHandle;
	const char* const name = descr->name;

	assert(descr->APIVersion == LibraryAPIUnknown);

	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetLibInfo", name);
	descr->GetLibInfo = (fpGetLibInfo_t)_getFunction(dlHandle, apiName);
	if (descr->GetLibInfo == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetLibVersion", name);
	descr->GetLibVersion = (fpGetLibVersion_t)_getFunction(dlHandle, apiName);
	if (descr->GetLibVersion == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetLastError", name);
	descr->GetLastError = (fpGetLastError_t)_getFunction(dlHandle, apiName);
	if (descr->GetLastError == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"DevicesDiscovery", name);
	descr->DevicesDiscovery = (fpDevicesDiscovery_t)_getFunction(dlHandle, apiName);
	if (descr->DevicesDiscovery == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"Open", name);
	descr->Open = (fpOpen_t)_getFunction(dlHandle, apiName);
	if (descr->Open == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"Close", name);
	descr->Close = (fpClose_t)_getFunction(dlHandle, apiName);
	if (descr->Close == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetDeviceTree", name);
	descr->GetDeviceTree = (fpGetDeviceTree_t)_getFunction(dlHandle, apiName);
	if (descr->GetDeviceTree == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetChildHandles", name);
	descr->GetChildHandles = (fpGetChildHandles_t)_getFunction(dlHandle, apiName);
	if (descr->GetChildHandles == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetHandle", name);
	descr->GetHandle = (fpGetHandle_t)_getFunction(dlHandle, apiName);
	if (descr->GetHandle == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetParentHandle", name);
	descr->GetParentHandle = (fpGetParentHandle_t)_getFunction(dlHandle, apiName);
	if (descr->GetParentHandle == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetPath", name);
	descr->GetPath = (fpGetPath_t)_getFunction(dlHandle, apiName);
	if (descr->GetPath == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetNodeProperties", name);
	descr->GetNodeProperties = (fpGetNodeProperties_t)_getFunction(dlHandle, apiName);
	if (descr->GetNodeProperties == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetValue", name);
	descr->GetValue = (fpGetValue_t)_getFunction(dlHandle, apiName);
	if (descr->GetValue == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"SetValue", name);
	descr->SetValue = (fpSetValue_t)_getFunction(dlHandle, apiName);
	if (descr->SetValue == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"SendCommand", name);
	descr->SendCommand = (fpSendCommand_t)_getFunction(dlHandle, apiName);
	if (descr->SendCommand == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"GetUserRegister", name);
	descr->GetUserRegister = (fpGetUserRegister_t)_getFunction(dlHandle, apiName);
	if (descr->GetUserRegister == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"SetUserRegister", name);
	descr->SetUserRegister = (fpSetUserRegister_t)_getFunction(dlHandle, apiName);
	if (descr->SetUserRegister == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"SetReadDataFormat", name);
	descr->SetReadDataFormat = (fpSetReadDataFormat_t)_getFunction(dlHandle, apiName);
	if (descr->SetReadDataFormat == NULL) {
		return CAEN_FELib_GenericError;
	}
	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"ReadDataV", name);
	descr->ReadDataV = (fpReadDataV_t)_getFunction(dlHandle, apiName);
	if (descr->ReadDataV == NULL) {
		return CAEN_FELib_GenericError;
	}

	descr->APIVersion = LibraryAPIv0;

	return CAEN_FELib_Success;
}

static int _loadAPIv1(struct library_descr* descr) {
	char apiName[64];
	const size_t apiNameSize = ARRAY_SIZE(apiName);
	const dlHandle_t dlHandle = descr->dlHandle;
	const char* const name = descr->name;

	assert(descr->APIVersion == LibraryAPIv0);

	snprintf(apiName, apiNameSize, CAEN_IMPL_API_PREFIX"HasData", name);
	descr->HasData = (fpHasData_t)_getFunction(dlHandle, apiName);
	if (descr->HasData == NULL) {
		return CAEN_FELib_GenericError;
	}

	descr->APIVersion = LibraryAPIv1;

	return CAEN_FELib_Success;
}

static void _getLastLocalError(char description[1024]) {
	strncpy(description, lastError, 1024);
	description[1024 - 1] = '\0';
}

static void _resetLastLocalError(void) {
	lastError[0] = '\0';
}

static void _setLastLocalError(const char* description, ...) {
	va_list args;
	va_start(args, description);
	// cppcheck-suppress sizeofDivisionMemfunc
	vsnprintf(lastError, ARRAY_SIZE(lastError), description, args);
	va_end(args);
}

static int _invalidHandle(void) {
	_setLastLocalError("invalid handle");
	return CAEN_FELib_InvalidHandle;
}

static int _notSupported(void) {
	_setLastLocalError("function not supported by the underlying library");
	return CAEN_FELib_NotImplemented;
}

static int _notImplemented(void) {
	_setLastLocalError("not yet implemented");
	return CAEN_FELib_NotImplemented;
}

static bool _checkAPI(struct library_descr* descr, enum library_api version) {
	return descr->APIVersion >= version;
}

int CAEN_FELIB_API CAEN_FELib_GetLibInfo(char* jsonString, size_t size) {
	return _notImplemented();
}

int CAEN_FELIB_API CAEN_FELib_GetLibVersion(char version[16]) {
	version[0] = '\0';
	strncat(version, CAEN_FELIB_VERSION_STRING, 16 - 1);
	return CAEN_FELib_Success;
}

static void* safe_memmove(void* dest, const void* src, size_t size) {
	return (dest != NULL) ? memmove(dest, src, size) : dest;
}
static void _errorStrcpy(char* destError, const char* srcError, size_t srcErrorSize, char* destDescr, const char* srcDescr, size_t srcDescrSize) {
	safe_memmove(destError, srcError, srcErrorSize);
	safe_memmove(destDescr, srcDescr, srcDescrSize);
}

#define GET_ERROR_CASE(ERROR_LITERAL, DESCRIPTION_LITERAL) \
do { \
	const char _err[] = ERROR_LITERAL; \
	const char _descr[] = DESCRIPTION_LITERAL; \
	STATIC_ASSERT(ARRAY_SIZE(_err) <= 32 && ARRAY_SIZE(_descr) <= 256, invalid_err_size); \
	_errorStrcpy(error, _err, ARRAY_SIZE(_err), description, _descr, ARRAY_SIZE(_descr)); \
} while (0)

static int _getError(CAEN_FELib_ErrorCode errorCode, char error[32], char description[256]) {
	switch (errorCode) {
	case CAEN_FELib_Success:
		GET_ERROR_CASE(
			"SUCCESS",
			"Success"
		);
		break;
	case CAEN_FELib_GenericError:
		GET_ERROR_CASE(
			"GENERIC ERROR",
			"Generic error"
		);
		break;
	case CAEN_FELib_InvalidParam:
		GET_ERROR_CASE(
			"INVALID PARAMETER",
			"One of the parameters used in the API has not been recognized by the library"
		);
		break;
	case CAEN_FELib_DeviceAlreadyOpen:
		GET_ERROR_CASE(
			"DEVICE ALREADY OPENED",
			"The device is already opened by this library, please close the connection before open a new one"
		);
		break;
	case CAEN_FELib_DeviceNotFound:
		GET_ERROR_CASE(
			"DEVICE NOT FOUND",
			"The device is not responding or it is not connected, verify that the path parameter is correct and the physical connection to the device"
		);
		break;
	case CAEN_FELib_MaxDevicesError:
		GET_ERROR_CASE(
			"TOO MANY DEVICES OPENED",
			"This library supports up to 256 simultaneous connections but seems tgat all the rooms are full. Please close a connection before open a new one"
		);
		break;
	case CAEN_FELib_CommandError:
		GET_ERROR_CASE(
			"COMMAND ERROR",
			"Command error"
		);
		break;
	case CAEN_FELib_InternalError:
		GET_ERROR_CASE(
			"INTERNAL ERROR",
			"Generic internal error"
		);
		break;
	case CAEN_FELib_NotImplemented:
		GET_ERROR_CASE(
			"API NOT IMPLEMENTED",
			"The API is not yet implemented in this library version, please try to upgrade the library to a new version"
		);
		break;
	case CAEN_FELib_InvalidHandle:
		GET_ERROR_CASE(
			"INVALID HANDLE",
			"The handle used is not a valid handle, it is related to a closed connection or it is not an handle of an item"
		);
		break;
	case CAEN_FELib_DeviceLibraryNotAvailable:
		GET_ERROR_CASE(
			"DEVICE LIBRARY NOT AVAILABLE",
			"Returned by CAEN_FELib_Open() in case of invalid prefix"
		);
		break;
	case CAEN_FELib_Timeout:
		GET_ERROR_CASE(
			"TIMEOUT",
			"Returned by CAEN_FELib_ReadData() in case of timeout"
		);
		break;
	case CAEN_FELib_Stop:
		GET_ERROR_CASE(
			"STOP",
			"Returned by CAEN_FELib_ReadData() after last event"
		);
		break;
	case CAEN_FELib_Disabled:
		GET_ERROR_CASE(
			"DISABLED",
			"Disabled function"
		);
		break;
	case CAEN_FELib_BadLibraryVersion:
		GET_ERROR_CASE(
			"BAD LIBRARY VERSION",
			"Returned by CAEN_FELib_Open() when succeeded, but the version of the underlying library is not aligned to the hardware"
		);
		break;
	case CAEN_FELib_CommunicationError:
		GET_ERROR_CASE(
			"COMMUNICATION ERROR",
			"Communication error"
		);
		break;
	default:
		_setLastLocalError("unknown error code '%d'", errorCode);
		return CAEN_FELib_InvalidParam;
	}
	return CAEN_FELib_Success;
}

#undef GET_ERROR_CASE

int CAEN_FELIB_API CAEN_FELib_GetErrorName(CAEN_FELib_ErrorCode error, char errorName[32]) {
	return _getError(error, errorName, NULL);
}

int CAEN_FELIB_API CAEN_FELib_GetErrorDescription(CAEN_FELib_ErrorCode error, char description[256]) {
	return _getError(error, NULL, description);
}

int CAEN_FELIB_API CAEN_FELib_GetLastError(char description[1024]) {
	_getLastLocalError(description);
	_resetLastLocalError();
	return CAEN_FELib_Success;
}

int CAEN_FELIB_API CAEN_FELib_DevicesDiscovery(char* jsonString, size_t size, int timeout) {
	char buff[FILENAME_MAX];
	struct dirent* entry;
	char* cd = getcwd(buff, FILENAME_MAX);
	if (cd == NULL) {
		_setLastLocalError("getcwd failed: %s", strerror(errno));
		return CAEN_FELib_GenericError;
	}
	DIR* dir = opendir(buff);
	if (dir == NULL) {
		_setLastLocalError("opendir failed : %s", strerror(errno));
		return CAEN_FELib_GenericError;
	}
	char* p = jsonString;
	size_t lsize = size;

	while ((entry = readdir(dir)) != NULL) {
		const char prefix[] = CAEN_IMPL_DLL_PREFIX;
		const char suffix[] = CAEN_IMPL_DLL_SUFFIX;
		if ((strncmp(entry->d_name, prefix, ARRAY_SIZE(prefix) - 1) == 0) &&
				(strstr(entry->d_name, suffix) != NULL) &&
				(_validateLibraryName(entry->d_name))) {
			char apiName[64];
			const dlHandle_t dlHandle = _loadLibrary(entry->d_name);
			if (dlHandle == NULL)
				continue;
			char* libName = _getLibraryHWName(entry->d_name); // this call modify entry->d_name
			snprintf(apiName, ARRAY_SIZE(apiName), CAEN_IMPL_API_PREFIX"DevicesDiscovery", libName);
			fpDevicesDiscovery_t devicesDiscovery = (fpDevicesDiscovery_t)_getFunction(dlHandle, apiName);
			snprintf(apiName, ARRAY_SIZE(apiName), CAEN_IMPL_API_PREFIX"GetLastError", libName);
			fpGetLastError_t getLastError = (fpGetLastError_t)_getFunction(dlHandle, apiName);
			if (devicesDiscovery == NULL || getLastError == NULL) {
				_closeLibrary(dlHandle);
				continue;
			}
			// TODO adjust how to concatenate multiple responses
			int res = devicesDiscovery(p, lsize, timeout);
			if (res != CAEN_FELib_Success) {
				getLastError(lastError); // save shared library last error before to close it
				_closeLibrary(dlHandle);
				closedir(dir);
				return res;
			}
			const size_t len = strlen(p);
			p += len;
			lsize -= len;
			_closeLibrary(dlHandle);
		}
	}
	closedir(dir);
	return CAEN_FELib_Success;
}

int CAEN_FELIB_API CAEN_FELib_Open(const char* url, uint64_t* handle) {
	int errCode;
	uint_fast16_t ch;
	uint_fast8_t lh;
	uint32_t rh;
	struct connection_descr lDescr;
	char libName[ARRAY_SIZE(libDescr[0]->name)];

	if (url == NULL || handle == NULL) {
		_setLastLocalError("NULL argument");
		return CAEN_FELib_InvalidParam;
	}

	char* const urlCopy = strdup(url);
	if (urlCopy == NULL) {
		_setLastLocalError("_duplicateString failed");
		return CAEN_FELib_GenericError;
	}

	struct hturi uri = _scanURI(urlCopy);

	if (uri.scheme == NULL) {
		_setLastLocalError("invalid URL '%s': scheme not found", url);
		free(urlCopy);
		return CAEN_FELib_InvalidParam;
	}

	const size_t libNameSize = strlen(uri.scheme);
	if (libNameSize == 0 || libNameSize >= ARRAY_SIZE(libName)) {
		_setLastLocalError("library name '%s': invalid size (%zu)", uri.scheme, libNameSize);
		free(urlCopy);
		return CAEN_FELib_InvalidParam;
	}

	/*
	 * This patch is required because initial versions of this library
	 * did not require "//" after scheme. If "//" is missing, _scan_uri()
	 * puts the content "relative" instead of "host"
	 */ 
	if (uri.host == NULL)
		uri.host = uri.relative;

	if (uri.host == NULL) {
		_setLastLocalError("invalid URL '%s': host not found", url);
		free(urlCopy);
		return CAEN_FELib_InvalidParam;
	}

	libName[0] = '\0';
	strncat(libName, uri.scheme, ARRAY_SIZE(libName) - 1);
	lDescr.arg[0] = '\0';
	strncat(lDescr.arg, uri.host, ARRAY_SIZE(lDescr.arg) - 1);

	// append optional absolute again, after "host/"
	if (uri.absolute != NULL) {
		const size_t currentLen = strlen(lDescr.arg);
		const size_t sizeLeft = ARRAY_SIZE(lDescr.arg) - currentLen - 1;
		if (sizeLeft != 0) {
			lDescr.arg[currentLen] = '/';
			lDescr.arg[currentLen + 1] = '\0';
			strncat(lDescr.arg, uri.absolute, sizeLeft - 1);
		}
	}

	free(urlCopy);

	// to make the URI case insensitive
	_adjustLibraryNameCase(libName, libNameSize);

	// find unused lh
	for (ch = 0; ch < ARRAY_SIZE(connectionDescr); ++ch)
		if (connectionDescr[ch] == NULL)
			break;

	if (ch == ARRAY_SIZE(connectionDescr)) {
		_setLastLocalError("too many devices (limited to %zu)", ARRAY_SIZE(connectionDescr));
		return CAEN_FELib_MaxDevicesError;
	}

	// check if the library is already open. if not found, open it
	for (lh = 0; lh < ARRAY_SIZE(libDescr); ++lh)
		if (libDescr[lh] != NULL && (strncmp(libDescr[lh]->name, libName, ARRAY_SIZE(libName)) == 0))
			break;

	struct library_descr* lib_descr = NULL;

	// if not found, open it
	if (lh == ARRAY_SIZE(libDescr)) {

		// find unused libDescr
		for (lh = 0; lh < ARRAY_SIZE(libDescr); ++lh)
			if (libDescr[lh] == NULL)
				break;

		if (lh == ARRAY_SIZE(libDescr)) {
			_setLastLocalError("too many different libraries (limited to %zu)", ARRAY_SIZE(libDescr));
			return CAEN_FELib_MaxDevicesError;
		}

		if (!_allocateLibDescr(lh)) {
			_setLastLocalError("_allocateLibDescr failed");
			return CAEN_FELib_InternalError;
		}

		lib_descr = libDescr[lh];

		const char libPattern[] = CAEN_IMPL_DLL_PREFIX"%s"CAEN_IMPL_DLL_SUFFIX;

		char libFileName[(ARRAY_SIZE(libName) - 1) + (ARRAY_SIZE(libPattern) - 2)]; // -1: null terminator of libName; -2: "%s" in libPattern
		snprintf(libFileName, ARRAY_SIZE(libFileName), libPattern, libName);
		lib_descr->dlHandle = _loadLibrary(libFileName);
		if (lib_descr->dlHandle == NULL) {
			_resetLibDescr(lh);
			_loadLibraryError(lastError, ARRAY_SIZE(lastError));
			return CAEN_FELib_DeviceLibraryNotAvailable;
		}

		// load filename
		strncpy(lib_descr->name, libName, ARRAY_SIZE(lib_descr->name));
		lib_descr->name[ARRAY_SIZE(lib_descr->name) - 1] = '\0';

		// load APIv0 (mandatory)
		errCode = _loadAPIv0(lib_descr);
		if (errCode != CAEN_FELib_Success) {
			_closeLibraryAndResetDevDescrIfLast(lh);
			_loadLibraryError(lastError, ARRAY_SIZE(lastError));
			return errCode;
		}

		// load APIv1 (optional)
		_loadAPIv1(lib_descr);

	} else {

		lib_descr = libDescr[lh];

	}

	assert(lib_descr != NULL);

	errCode = lib_descr->Open(lDescr.arg, &rh);

	if (errCode != CAEN_FELib_Success)
		lib_descr->GetLastError(lastError); // save error before close lib

	if (errCode != CAEN_FELib_Success && errCode != CAEN_FELib_BadLibraryVersion) {
		_closeLibraryAndResetDevDescrIfLast(lh);
		return errCode;
	}

	if (!_allocateConnectionDescr(ch)) {
		_closeLibraryAndResetDevDescrIfLast(lh);
		_setLastLocalError("_allocateConnectionDescr failed");
		return CAEN_FELib_InternalError;
	}

	struct connection_descr* conn_descr = connectionDescr[ch];

	assert(conn_descr != NULL);

	strncpy(conn_descr->arg, lDescr.arg, ARRAY_SIZE(conn_descr->arg));
	conn_descr->arg[ARRAY_SIZE(conn_descr->arg) - 1] = '\0';
	conn_descr->lHandle = lh;

	lib_descr->nRef++;

	*handle = _handle(ch, rh);

	return errCode;
}

int CAEN_FELIB_API CAEN_FELib_Close(uint64_t handle) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->Close(rHandle);
	if (ret == CAEN_FELib_Success) {
		descr->nRef--;
		const uint_fast16_t cHandle = _cHandle(handle);
		_closeLibraryAndResetDevDescrIfLast(connectionDescr[cHandle]->lHandle);
		_resetConnectionDescr(cHandle);
	} else {
		descr->GetLastError(lastError);
	}
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetImplLibVersion(uint64_t handle, char version[16]) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const int ret = descr->GetLibVersion(version);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetDeviceTree(uint64_t handle, char* jsonString, size_t size) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->GetDeviceTree(rHandle, jsonString, size);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetChildHandles(uint64_t handle, const char* path, uint64_t* handles, size_t size) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	/*
	 * Returned uint32_t handles must be converted to uint64_t handles.
	 *
	 * There can be at least 3 approaches:
	 * 1. allocate a uint32_t buffer, copy using a forward loop, free the buffer
	 * 2. use the higher half of the user memory, copy using a forward loop
	 * 3. use the lower half of the user memory, copy using a backward loop
	 *
	 * The first approach is the slowest because requires malloc/free. The second is
	 * the fastest because forward loops are also more efficiently optimized by
	 * compilers. The price to pay is that the user would see modified content in
	 * memory also after last returned valid handle, in the likely case that passed
	 * size is larger than returned size. Everything considered, the last approach
	 * seems the best compromise.
	 */
	uint32_t* tHandles = (uint32_t*)handles;
	const int ret = descr->GetChildHandles(rHandle, path, tHandles, size);
	if (ret >= 0) {
		const size_t retSize = (size_t)ret;
		const size_t minSize = (retSize < size) ? retSize : size;
		const uint_fast16_t cHandle = _cHandle(handle);
		for (size_t i = minSize; i--;)
			handles[i] = _handle(cHandle, tHandles[i]);
	} else {
		descr->GetLastError(lastError);
	}
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetHandle(uint64_t handle, const char* path, uint64_t* pathHandle) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	uint32_t tHandle;
	const int ret = descr->GetHandle(rHandle, path, &tHandle);
	if (ret == CAEN_FELib_Success) {
		const uint_fast16_t cHandle = _cHandle(handle);
		*pathHandle = _handle(cHandle, tHandle);
	} else {
		descr->GetLastError(lastError);
	}
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetParentHandle(uint64_t handle, const char* path, uint64_t* parentHandle) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	uint32_t tHandle;
	const int ret = descr->GetParentHandle(rHandle, path, &tHandle);
	if (ret == CAEN_FELib_Success) {
		const uint_fast16_t cHandle = _cHandle(handle);
		*parentHandle = _handle(cHandle, tHandle);
	} else {
		descr->GetLastError(lastError);
	}
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetPath(uint64_t handle, char path[256]) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->GetPath(rHandle, path);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetNodeProperties(uint64_t handle, const char* path, char name[32], CAEN_FELib_NodeType_t*type) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->GetNodeProperties(rHandle, path, name, type);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetValue(uint64_t handle, const char* path, char value[256]) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->GetValue(rHandle, path, value);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_SetValue(uint64_t handle, const char* path, const char* value) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->SetValue(rHandle, path, value);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_SendCommand(uint64_t handle, const char* path) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->SendCommand(rHandle, path);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_GetUserRegister(uint64_t handle, uint32_t address, uint32_t* value) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->GetUserRegister(rHandle, address, value);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_SetUserRegister(uint64_t handle, uint32_t address, uint32_t value) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->SetUserRegister(rHandle, address, value);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_SetReadDataFormat(uint64_t handle, const char* jsonString) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->SetReadDataFormat(rHandle, jsonString);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_ReadData(uint64_t handle, int timeout, ...) {
	va_list args;
	va_start(args, timeout);
	const int ret = CAEN_FELib_ReadDataV(handle, timeout, args);
	va_end(args);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_ReadDataV(uint64_t handle, int timeout, va_list args) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv0))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->ReadDataV(rHandle, timeout, args);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

int CAEN_FELIB_API CAEN_FELib_HasData(uint64_t handle, int timeout) {
	struct library_descr* const descr = _getLibDescr(handle);
	if (descr == NULL)
		return _invalidHandle();
	if (!_checkAPI(descr, LibraryAPIv1))
		return _notSupported();
	const uint32_t rHandle = _rHandle(handle);
	const int ret = descr->HasData(rHandle, timeout);
	if (ret != CAEN_FELib_Success)
		descr->GetLastError(lastError);
	return ret;
}

// perform here any library initialization.
static void init_library(void) {
	for (uint_fast16_t i = 0; i < ARRAY_SIZE(connectionDescr); ++i)
		connectionDescr[i] = NULL;
	for (uint_fast8_t i = 0; i < ARRAY_SIZE(libDescr); ++i)
		libDescr[i] = NULL;
}

// perform here any library deinitialization.
static void deinit_library(void) {
	for (uint_fast16_t i = 0; i < ARRAY_SIZE(connectionDescr); ++i) {
		if (_isValid(i)) {
			const uint_fast8_t lHandle = connectionDescr[i]->lHandle;
			libDescr[lHandle]->nRef--;
			_closeLibraryAndResetDevDescrIfLast(lHandle);
			_resetConnectionDescr(i);
		}
	}
	// at this poing libDescr has already been cleared.
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		/*
		 * DllMain documentation suggests to call DisableThreadLibraryCalls on DLL_PROCESS_ATTACH,
		 * if not linking against static CRT (and this is not the case).
		 *
		 * From DisableThreadLibraryCalls documentation:
		 * > This can be a useful optimization for multithreaded applications that have many DLLs,
		 * > frequently create and delete threads, and whose DLLs do not need these thread-level
		 * > notifications of attachment/detachment.
		 *
		 * Anyway, still from documentation:
		 * > This function does not perform any optimizations if static Thread Local Storage (TLS)
		 * > is enabled. Static TLS is enabled when using thread_local variables, __declspec(thread)
		 * > variables, or function-local static.
		 *
		 * TLS is enabled in this library because it is used at least by:
		 * - last error string (declared with `thread_local` storage duration)
		 *
		 * However, even if it does not perform any optimization here, we keep this as memorandum.
		 *
		 * See:
		 * - https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain
		 * - https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-disablethreadlibrarycalls
		 */
		DisableThreadLibraryCalls(hinstDLL);
		init_library();
		break;
	case DLL_PROCESS_DETACH:
		if (lpReserved != NULL) {
			 /*
			  * If lpReserved is not NULL it means that the process is terminating: weird things
			  * may happen here if there were running threads. We set a global flag that can be
			  * useful on class destructors to perform some consistency checks.
			  *
			  * DllMain documentation suggests that:
			  * > In this case, it is not safe for the DLL to clean up the resources.
			  * > Instead, the DLL should allow the operating system to reclaim the memory.
			  *
			  * To avoid problems, we do not invoke deinit_library().
			  *
			  * See:
			  * - https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain
			  */
			return TRUE;
		}
		deinit_library();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}
#else
__attribute__((constructor)) static void gnu_lib_constructor(void) {
	init_library();
}
__attribute__((destructor)) static void gnu_lib_destructor(void) {
	deinit_library();
}
#endif
