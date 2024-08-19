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
*	\file		definitions.h
*	\brief		Definition of types for internal machinery
*	\author		Giovanni Cerretani, Matteo Fusco, Alberto Lucchesi
*
******************************************************************************/

#ifndef CAEN_INCLUDE_DEFINITIONS_H_
#define CAEN_INCLUDE_DEFINITIONS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "CAEN_FELib.h"

typedef int (CAEN_FELIB_API* fpGetLibInfo_t)(char* jsonString, size_t size);
typedef int (CAEN_FELIB_API* fpGetLibVersion_t)(char version[16]);
typedef int (CAEN_FELIB_API* fpGetLastError_t)(char description[1024]);
typedef int (CAEN_FELIB_API* fpDevicesDiscovery_t)(char* jsonString, size_t size, int timeout);
typedef int (CAEN_FELIB_API* fpOpen_t)(const char* path, uint32_t* handle);
typedef int (CAEN_FELIB_API* fpClose_t)(uint32_t handle);
typedef int (CAEN_FELIB_API* fpGetDeviceTree_t)(uint32_t handle, char* jsonString, size_t size);
typedef int (CAEN_FELIB_API* fpGetChildHandles_t)(uint32_t handle, const char* path, uint32_t* handles, size_t size);
typedef int (CAEN_FELIB_API* fpGetHandle_t)(uint32_t handle, const char* path, uint32_t* parentHandle);
typedef int (CAEN_FELIB_API* fpGetParentHandle_t)(uint32_t handle, const char* path, uint32_t* parentHandle);
typedef int (CAEN_FELIB_API* fpGetPath_t)(uint32_t handle, char path[256]);
typedef int (CAEN_FELIB_API* fpGetNodeProperties_t)(uint32_t handle, const char* path, char name[32], CAEN_FELib_NodeType_t* type);
typedef int (CAEN_FELIB_API* fpGetValue_t)(uint32_t handle, const char* path, char value[256]);
typedef int (CAEN_FELIB_API* fpSetValue_t)(uint32_t handle, const char* path, const char* value);
typedef int (CAEN_FELIB_API* fpSendCommand_t)(uint32_t handle, const char* query);
typedef int (CAEN_FELIB_API* fpGetUserRegister_t)(uint32_t handle, uint32_t address, uint32_t* value);
typedef int (CAEN_FELIB_API* fpSetUserRegister_t)(uint32_t handle, uint32_t address, uint32_t value);
typedef int (CAEN_FELIB_API* fpSetReadDataFormat_t)(uint32_t handle, const char* jsonString);
typedef int (CAEN_FELIB_API* fpReadDataV_t)(uint32_t handle, int timeout, va_list args);
typedef int (CAEN_FELIB_API* fpHasData_t)(uint32_t handle, int timeout);

#ifdef _WIN32
typedef HMODULE						dlHandle_t;
typedef FARPROC						dlSymbol_t;
#else
typedef	void*						dlHandle_t;
typedef void*						dlSymbol_t;
#endif

struct connection_descr {
	char							arg[128];
	uint_fast8_t					lHandle;
};

enum library_api {
	LibraryAPIUnknown,
	LibraryAPIv0,
	LibraryAPIv1,
};

struct library_descr {
	char							name[16];
	enum library_api				APIVersion;
	uint_fast16_t					nRef;
	dlHandle_t						dlHandle;
	// API v0
	fpGetLibInfo_t					GetLibInfo;
	fpGetLibVersion_t				GetLibVersion;
	fpGetLastError_t				GetLastError;
	fpDevicesDiscovery_t			DevicesDiscovery;
	fpOpen_t						Open;
	fpClose_t						Close;
	fpGetDeviceTree_t				GetDeviceTree;
	fpGetChildHandles_t				GetChildHandles;
	fpGetHandle_t					GetHandle;
	fpGetParentHandle_t				GetParentHandle;
	fpGetPath_t						GetPath;
	fpGetNodeProperties_t			GetNodeProperties;
	fpGetValue_t					GetValue;
	fpSetValue_t					SetValue;
	fpSendCommand_t					SendCommand;
	fpGetUserRegister_t				GetUserRegister;
	fpSetUserRegister_t				SetUserRegister;
	fpSetReadDataFormat_t			SetReadDataFormat;
	fpReadDataV_t					ReadDataV;
	// API v1
	fpHasData_t						HasData;
};

#endif /* CAEN_INCLUDE_DEFINITIONS_H_ */
