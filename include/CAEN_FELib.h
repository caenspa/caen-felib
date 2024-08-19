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
*	\file		CAEN_FELib.h
*	\brief		CAEN FE Library
*	\author		Giovanni Cerretani, Matteo Fusco, Alberto Lucchesi
*
******************************************************************************/

#ifndef CAEN_INCLUDE_CAEN_FELIB_H_
#define CAEN_INCLUDE_CAEN_FELIB_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*!
* @defgroup UtilityMacros Utility macros
* @brief Generic macros.
* @{ */
#define CAEN_FELIB_STR_HELPER_(S)			#S
#define CAEN_FELIB_STR(S)					CAEN_FELIB_STR_HELPER_(S)
#define CAEN_FELIB_DEPRECATED_MSG(V, R)		"Deprecated since " #V ". Use " #R
#ifdef _WIN32 // Windows
#define CAEN_FELIB_API						__stdcall
#define CAEN_FELIB_DLLAPI
#define CAEN_FELIB_DEPRECATED(V, R, ...)	__declspec(deprecated(CAEN_FELIB_DEPRECATED_MSG(V, R))) __VA_ARGS__
#else // Linux
#define CAEN_FELIB_API
#define CAEN_FELIB_DLLAPI					__attribute__((visibility("default")))
#define CAEN_FELIB_DEPRECATED(V, R, ...)	__VA_ARGS__ __attribute__((deprecated(CAEN_FELIB_DEPRECATED_MSG(V, R))))
#endif
/*! @} */

/*!
* @defgroup VersionMacros Version macros
* @brief Macros to define the library version.
* @{ */
#define CAEN_FELIB_VERSION_MAJOR			1
#define CAEN_FELIB_VERSION_MINOR			3
#define CAEN_FELIB_VERSION_PATCH			1
#define CAEN_FELIB_VERSION					(CAEN_FELIB_VERSION_MAJOR * 10000) + (CAEN_FELIB_VERSION_MINOR * 100) + (CAEN_FELIB_VERSION_PATCH)
#define CAEN_FELIB_VERSION_STRING			CAEN_FELIB_STR(CAEN_FELIB_VERSION_MAJOR) "." CAEN_FELIB_STR(CAEN_FELIB_VERSION_MINOR) "." CAEN_FELIB_STR(CAEN_FELIB_VERSION_PATCH)
/*! @} */

/**
* @defgroup Enums Enumerations
* @brief Application enumerations.
*/

/**
* @defgroup Functions API
* @brief Application programming interface.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes.
 *
 * @ingroup Enums
 */
typedef enum {
	CAEN_FELib_Success						= 0,	//!< Operation completed successfully
	CAEN_FELib_GenericError					= -1,	//!< Unspecified error
	CAEN_FELib_InvalidParam					= -2,	//!< Invalid parameter
	CAEN_FELib_DeviceAlreadyOpen			= -3,	//!< Returned by CAEN_FELib_Open() in case of device already opened
	CAEN_FELib_DeviceNotFound				= -4,	//!< Returned by CAEN_FELib_Open() in case of device not found
	CAEN_FELib_MaxDevicesError				= -5,	//!< Returned by CAEN_FELib_Open() in case of maximum number of devices exceeded
	CAEN_FELib_CommandError					= -6,	//!< Command error
	CAEN_FELib_InternalError				= -7,	//!< Internal error
	CAEN_FELib_NotImplemented				= -8,	//!< Not yet implemented function
	CAEN_FELib_InvalidHandle				= -9,	//!< Invalid handle
	CAEN_FELib_DeviceLibraryNotAvailable	= -10,	//!< Returned by CAEN_FELib_Open() in case of invalid prefix
	CAEN_FELib_Timeout						= -11,	//!< Returned by CAEN_FELib_ReadData() and CAEN_FELib_HasData() in case of timeout
	CAEN_FELib_Stop							= -12,	//!< Returned by CAEN_FELib_ReadData() and CAEN_FELib_HasData() once after the last event of a run
	CAEN_FELib_Disabled						= -13,	//!< Disabled function
	CAEN_FELib_BadLibraryVersion			= -14,	//!< Returned by CAEN_FELib_Open() in case of library version not supported by the server
	CAEN_FELib_CommunicationError			= -15,	//!< Communication error
} CAEN_FELib_ErrorCode;

/**
 * @brief Node types.
 *
 * @ingroup Enums
 */
typedef enum {
	CAEN_FELib_UNKNOWN		= -1,		//!< Unknown
	CAEN_FELib_PARAMETER	= 0,		//!< Parameter
	CAEN_FELib_COMMAND		= 1,		//!< Command
	CAEN_FELib_FEATURE		= 2,		//!< Feature
	CAEN_FELib_ATTRIBUTE	= 3,		//!< Attribute
	CAEN_FELib_ENDPOINT		= 4,		//!< Endpoint
	CAEN_FELib_CHANNEL		= 5,		//!< Channel
	CAEN_FELib_DIGITIZER	= 6,		//!< Digitizer
	CAEN_FELib_FOLDER		= 7,		//!< Folder
	CAEN_FELib_LVDS			= 8,		//!< LVDS
	CAEN_FELib_VGA			= 9,		//!< VGA
	CAEN_FELib_HV_CHANNEL	= 10,		//!< HV Channel
	CAEN_FELib_MONOUT		= 11,		//!< MonOut
	CAEN_FELib_VTRACE		= 12,		//!< Virtual Traces
	CAEN_FELib_GROUP		= 13,		//!< Group
	CAEN_FELib_HV_RANGE		= 14,		//!< HV Range
} CAEN_FELib_NodeType_t;

/**
 * @brief Get a JSON string that contains informations about this library, like version, supported devices, etc.
 *
 * @param[out] jsonString		JSON representation of the library info (null-terminated string)
 * @param[in] size				size of @p jsonString array
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetLibInfo(char* jsonString, size_t size);

/**
 * @brief Get the value of #CAEN_FELIB_VERSION_STRING used to compile the library.
 *
 * @param[out] version 			version (null-terminated string) [max size: 16 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetLibVersion(char version[16]);

/**
 * @brief Get the short name of an error code.
 *
 * @param[in] error				error code returned by library functions
 * @param[out] errorName		error name (null-terminated string) [max size: 32 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetErrorName(CAEN_FELib_ErrorCode error, char errorName[32]);

/**
 * @brief Get the detailed description of an error code.
 *
 * @param[in] error				error code returned by library functions
 * @param[out] description		error extended description (null-terminated string) [max size: 256 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetErrorDescription(CAEN_FELib_ErrorCode error, char description[256]);

/**
 * @brief Get the detailed description of the last error occurred on the current thread for a device.
 *
 * @param[out] description		last error description (null-terminated string) [max size: 1024 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetLastError(char description[1024]);

/**
 * @brief Discover the connected devices that can be managed by the library.
 *
 * @param[out] jsonString		JSON representation of the devices found (null-terminated string)
 * @param[in] size				size of @p jsonString array
 * @param[in] timeout			timeout of the function in seconds
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_DevicesDiscovery(char* jsonString, size_t size, int timeout);

/**
 * @brief Connect to a device.
 *
 * @param[in] url				URL of device to connect (null-terminated string, with standard format `[scheme]:[//authority][path][?query][#fragment]`)
 * @param[out] handle			root handle, representing a node of type ::CAEN_FELib_DIGITIZER
 * @retval						::CAEN_FELib_Success (0) in case of success
 * @retval						::CAEN_FELib_BadLibraryVersion in case of success, but using a old version of the underlying library that may not support some new features of the hardware
 * @retval						or a negative error code specified in #CAEN_FELib_ErrorCode
 * @note When returning ::CAEN_FELib_BadLibraryVersion the connection has been properly established, but there could be unexpected behaviours: the user should update the underlying library as soon as possible.
 * @warning CAEN_FELib_Open() and CAEN_FELib_Close() modify a static variable: are not thread safe.
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_Open(const char* url, uint64_t* handle);

/**
 * @brief Close the connection with device.
 * @nodetype ::CAEN_FELib_DIGITIZER
 * 
 * @param[in] handle			handle
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @warning CAEN_FELib_Open() and CAEN_FELib_Close() modify a static variable: are not thread safe.
 * @warning CAEN_FELib_Close() should never be called if there are pending calls on handles related to the device that is going to be closed.
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_Close(uint64_t handle);

/**
 * @brief Get the version of the implementation library used by an handle.
 *
 * @param[in] handle			handle
 * @param[out] version 			version (null-terminated string) [max size: 16 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetImplLibVersion(uint64_t handle, char version[16]);

/**
 * @brief Get the tree that describe all the structure of the device in JSON format.
 *
 * @param[in] handle			handle
 * @param[out] jsonString		JSON representation of the node structure (null-terminated string, can be null if @p size is zero)
 * @param[in] size				size of @p jsonString array
 * @return						number of characters that would have been written for a sufficiently large @p jsonString if successful (not including the terminating null character), or a negative error code specified in #CAEN_FELib_ErrorCode
 * @note The output @p jsonString has been completely written if and only if the returned value is in range [0, @p size)
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetDeviceTree(uint64_t handle, char* jsonString, size_t size);

/**
 * @brief Get handles that represent nodes of the tree.
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[out] handles			child handles (can be null if @p size is zero)
 * @param[in] size				size of @p handles array
 * @return						number of handles that would have been written for a sufficiently large @p handles if successful, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @note The output @p handles has been completely written if and only if the returned value is in range [0, @p size]
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetChildHandles(uint64_t handle, const char* path, uint64_t* handles, size_t size);

/**
 * @brief Get the handle of a node.
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[out] pathHandle		handle of the provided path
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetHandle(uint64_t handle, const char* path, uint64_t* pathHandle);

/**
 * @brief Get the parent handle of a node.
 * @nodetype any except ::CAEN_FELib_DIGITIZER
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[out] parentHandle		handle that is the parent of the provided handle
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetParentHandle(uint64_t handle, const char* path, uint64_t* parentHandle);

/**
 * @brief Get the absolute path of a node.
 *
 * @param[in] handle			handle
 * @param[out] path				absolute path of the provided handle (null-terminated string) [max size: 256 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetPath(uint64_t handle, char path[256]);

/**
 * @brief Get the name and type of a node.
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[out] name				name of the node (null-terminated string) [max size: 32 bytes]
 * @param[out] type				type of the node (see ::CAEN_FELib_NodeType_t description)
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetNodeProperties(uint64_t handle, const char* path, char name[32], CAEN_FELib_NodeType_t* type);

/**
 * @brief Get the value of a readable node.
 * @nodetype ::CAEN_FELib_PARAMETER ::CAEN_FELib_ATTRIBUTE ::CAEN_FELib_FEATURE
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[out] value			value of the node (null-terminated string) [max size: 256 bytes]
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetValue(uint64_t handle, const char* path, char value[256]);

/**
 * @brief Set the value of a writable node.
 * @nodetype ::CAEN_FELib_PARAMETER
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @param[in] value				value to set (null-terminated string)
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_SetValue(uint64_t handle, const char* path, const char* value);

/**
 * @brief Get the value of a user register.
 * @nodetype ::CAEN_FELib_DIGITIZER
 *
 * @param[in] handle			handle
 * @param[in] address			user register address
 * @param[out] value			value of the register
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_GetUserRegister(uint64_t handle, uint32_t address, uint32_t* value);

/**
 * @brief Set the value of a user register.
 * @nodetype ::CAEN_FELib_DIGITIZER
 *
 * @param[in] handle			handle
 * @param[in] address			user register address
 * @param[in] value				value of the register
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_SetUserRegister(uint64_t handle, uint32_t address, uint32_t value);

/**
 * @brief Send a command specified by a node.
 * @nodetype ::CAEN_FELib_COMMAND
 *
 * @param[in] handle			handle
 * @param[in] path				relative path of a node with respect to @p handle (either a null-terminated string or a null pointer that is interpreted as an empty string)
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_SendCommand(uint64_t handle, const char* path);

/**
 * @brief Set the format for the ReadData function to a endpoint node.
 * @nodetype ::CAEN_FELib_ENDPOINT
 *
 * @param[in] handle			handle
 * @param[in] jsonString		JSON representation of the format, in compliance with the endpoint "format" property (null-terminated string)
 * @return						::CAEN_FELib_Success (0) in case of success, or a negative error code specified in #CAEN_FELib_ErrorCode
 * @warning CAEN_FELib_SetReadDataFormat() modify a static, handle-specific variable: is not thread safe (but is thread safe if called on different handles).
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_SetReadDataFormat(uint64_t handle, const char* jsonString);

/**
 * @brief Read the data provided by an endpoint node.
 * @nodetype ::CAEN_FELib_ENDPOINT
 *
 * @param[in] handle			handle
 * @param[in] timeout			timeout of the function in milliseconds; if this value is -1 the function is blocking with infinite timeout
 * @param[out] ...				sequence of pointers to variable specified by a previous call to CAEN_FELib_SetReadDataFormat()
 * @retval						::CAEN_FELib_Success (0) in case of success
 * @retval						::CAEN_FELib_Timeout in case of timeout
 * @retval						::CAEN_FELib_Stop once after the last event of a run (if available; see endpoint documentation)
 * @retval						or a negative error code specified in #CAEN_FELib_ErrorCode
 * @warning There can be only one pending call of CAEN_FELib_HasData() and CAEN_FELib_ReadData() on the same handle; an error is returned by the second invocation.
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_ReadData(uint64_t handle, int timeout, ...);

/**
 * @brief Read the data provided by an endpoint node.
 * 
 * Identical to CAEN_FELib_ReadData(), using variable argument list.
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_ReadDataV(uint64_t handle, int timeout, va_list args);

/**
 * @brief Check if an endpoint node has data to be read with a subsequent call to CAEN_FELib_ReadData().
 * @nodetype ::CAEN_FELib_ENDPOINT
 *
 * @param[in] handle			handle
 * @param[in] timeout			timeout of the function in milliseconds; if this value is -1 the function is blocking with infinite timeout
 * @retval						::CAEN_FELib_Success (0) in case of success
 * @retval						::CAEN_FELib_Timeout in case of timeout
 * @retval						::CAEN_FELib_Stop once after the last event of a run (if available; see endpoint documentation)
 * @retval						or a negative error code specified in #CAEN_FELib_ErrorCode
 * @warning There can be only one pending call of CAEN_FELib_HasData() and CAEN_FELib_ReadData() on the same handle; an error is returned by the second invocation.
 * @ingroup Functions
 */
CAEN_FELIB_DLLAPI int CAEN_FELIB_API CAEN_FELib_HasData(uint64_t handle, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* CAEN_INCLUDE_CAEN_FELIB_H_ */
