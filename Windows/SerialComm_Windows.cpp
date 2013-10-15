/*
 * SerialComm_Windows.cpp
 *
 *       Created on:  Feb 25, 2012
 *  Last Updated on:  Mar 14, 2013
 *           Author:  Will Hedgecock
 *
 * Copyright (C) 2012-2013 Will Hedgecock
 *
 * This file is part of SerialComm.
 *
 * SerialComm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SerialComm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with SerialComm.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#define WINVER _WIN32_WINNT_WINXP
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXP
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>
#include "../j_extensions_comm_SerialComm.h"

JNIEXPORT jobjectArray JNICALL Java_j_extensions_comm_SerialComm_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	DWORD numValues, valueLength, comPortLength;
	HKEY keyHandle;
	CHAR valueName[1024];
	BYTE comPort[1024];

	// Get relevant SerialComm methods
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");

	// Enumerate serial ports on machine
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle);
	RegQueryInfoKey(keyHandle, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, NULL, NULL, NULL);
	jobjectArray arrayObject = env->NewObjectArray(numValues, serialCommClass, 0);
	for (DWORD i = 0; i < numValues; ++i)
	{
		// Get serial port name and COM value
		valueLength = comPortLength = 1024;
		RegEnumValue(keyHandle, i, valueName, &valueLength, NULL, NULL, comPort, &comPortLength);

		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);

		// Set port name and description
		if (comPort[0] != '\\')
		{
			char comPrefix[1024] = "\\\\.\\";
			strcat(comPrefix, (char*)comPort);
			env->SetObjectField(serialCommObject, env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;"), env->NewStringUTF(comPrefix));
		}
		else
			env->SetObjectField(serialCommObject, env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;"), env->NewStringUTF((char*)comPort));
		env->SetObjectField(serialCommObject, env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;"), env->NewStringUTF(strrchr(valueName, '\\') + 1));

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
	}

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	jclass serialCommClass = env->GetObjectClass(obj);
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;"));
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((serialPortHandle = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL)) != INVALID_HANDLE_VALUE)
	{
		// Set port handle in Java structure
		env->SetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"), (jlong)serialPortHandle);

		// Configure the port parameters and timeouts
		if (Java_j_extensions_comm_SerialComm_configPort(env, obj) && Java_j_extensions_comm_SerialComm_configFlowControl(env, obj) &&
				Java_j_extensions_comm_SerialComm_configTimeouts(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			CloseHandle(serialPortHandle);
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (serialPortHandle == INVALID_HANDLE_VALUE) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configPort(JNIEnv *env, jobject obj)
{
	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength = sizeof(DCB);
	jclass serialCommClass = env->GetObjectClass(obj);

	// Get port parameters from Java class
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	DWORD baudRate = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	BYTE byteSize = (BYTE)env->GetIntField(obj, env->GetFieldID(serialCommClass, "dataBits", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	BYTE stopBits = (stopBitsInt == j_extensions_comm_SerialComm_ONE_STOP_BIT) ? ONESTOPBIT : (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_STOP_BITS) ? ONE5STOPBITS : TWOSTOPBITS;
	BYTE parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? NOPARITY : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? ODDPARITY : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? EVENPARITY : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? MARKPARITY : SPACEPARITY;
	BOOL isParity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? FALSE : TRUE;

	// Retrieve existing port configuration
	if (!GetCommState(serialPortHandle, &dcbSerialParams))
		return JNI_FALSE;

	// Set updated port parameters
	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = byteSize;
	dcbSerialParams.StopBits = stopBits;
	dcbSerialParams.Parity = parity;
	dcbSerialParams.fParity = isParity;
	dcbSerialParams.fBinary = TRUE;
	dcbSerialParams.fAbortOnError = FALSE;

	// Apply changes
	return SetCommState(serialPortHandle, &dcbSerialParams);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configFlowControl(JNIEnv *env, jobject obj)
{
	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength = sizeof(DCB);
	jclass serialCommClass = env->GetObjectClass(obj);
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));

	// Get flow control parameters from Java class
	int flowControl = env->GetIntField(obj, env->GetFieldID(serialCommClass, "flowControl", "I"));
	BOOL CTSEnabled = (((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_CTS_ENABLED) > 0) ||
			((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_RTS_ENABLED) > 0));
	BOOL DSREnabled = (((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_DSR_ENABLED) > 0) ||
			((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_DTR_ENABLED) > 0));
	BYTE DTRValue = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_DTR_ENABLED) > 0) ? DTR_CONTROL_HANDSHAKE : DTR_CONTROL_ENABLE;
	BYTE RTSValue = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_RTS_ENABLED) > 0) ? RTS_CONTROL_HANDSHAKE : RTS_CONTROL_ENABLE;
	BOOL XonXoffInEnabled = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0);
	BOOL XonXoffOutEnabled = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0);

	// Retrieve existing port configuration
	if (!GetCommState(serialPortHandle, &dcbSerialParams))
		return JNI_FALSE;

	// Set updated port parameters
	dcbSerialParams.fRtsControl = RTSValue;
	dcbSerialParams.fOutxCtsFlow = CTSEnabled;
	dcbSerialParams.fOutxDsrFlow = DSREnabled;
	dcbSerialParams.fDtrControl = DTRValue;
	dcbSerialParams.fDsrSensitivity = DSREnabled;
	dcbSerialParams.fOutX = XonXoffOutEnabled;
	dcbSerialParams.fInX = XonXoffInEnabled;
	dcbSerialParams.fTXContinueOnXoff = TRUE;
	dcbSerialParams.fErrorChar = FALSE;
	dcbSerialParams.fNull = FALSE;
	dcbSerialParams.fAbortOnError = FALSE;
	dcbSerialParams.XonLim = 2048;
	dcbSerialParams.XoffLim = 512;
	dcbSerialParams.XonChar = (char)17;
	dcbSerialParams.XoffChar = (char)19;

	// Apply changes
	return SetCommState(serialPortHandle, &dcbSerialParams);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configTimeouts(JNIEnv *env, jobject obj)
{
	// Get port timeouts from Java class
	COMMTIMEOUTS timeouts = {0};
	jclass serialCommClass = env->GetObjectClass(obj);
	HANDLE serialHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	int timeoutMode = env->GetIntField(obj, env->GetFieldID(serialCommClass, "timeoutMode", "I"));
	DWORD readTimeout = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));
	DWORD writeTimeout = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "writeTimeout", "I"));

	// Set updated port timeouts
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	switch (timeoutMode)
	{
		case j_extensions_comm_SerialComm_TIMEOUT_READ_SEMI_BLOCKING:		// Read Semi-blocking
			timeouts.ReadIntervalTimeout = 10;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = 10;
			break;
		case (j_extensions_comm_SerialComm_TIMEOUT_READ_SEMI_BLOCKING | j_extensions_comm_SerialComm_TIMEOUT_WRITE_SEMI_BLOCKING):	// Read/Write Semi-blocking
			timeouts.ReadIntervalTimeout = 10;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case (j_extensions_comm_SerialComm_TIMEOUT_READ_SEMI_BLOCKING | j_extensions_comm_SerialComm_TIMEOUT_WRITE_BLOCKING):		// Read Semi-blocking/Write Blocking
			timeouts.ReadIntervalTimeout = 10;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case j_extensions_comm_SerialComm_TIMEOUT_READ_BLOCKING:		// Read Blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = 10;
			break;
		case (j_extensions_comm_SerialComm_TIMEOUT_READ_BLOCKING | j_extensions_comm_SerialComm_TIMEOUT_WRITE_SEMI_BLOCKING):	// Read Blocking/Write Semi-blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case (j_extensions_comm_SerialComm_TIMEOUT_READ_BLOCKING | j_extensions_comm_SerialComm_TIMEOUT_WRITE_BLOCKING):		// Read/Write Blocking
			timeouts.ReadIntervalTimeout = 0;
			timeouts.ReadTotalTimeoutConstant = readTimeout;
			timeouts.WriteTotalTimeoutConstant = writeTimeout;
			break;
		case j_extensions_comm_SerialComm_TIMEOUT_NONBLOCKING:		// Non-blocking
		default:
			timeouts.ReadIntervalTimeout = MAXDWORD;
			timeouts.ReadTotalTimeoutConstant = 0;
			timeouts.WriteTotalTimeoutConstant = 10;
			break;
	}

	// Apply changes
	return SetCommTimeouts(serialHandle, &timeouts);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_closePort(JNIEnv *env, jobject obj)
{
	// Purge any outstanding port operations
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	PurgeComm(serialPortHandle, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR);

	// Close port
	BOOL retVal = CloseHandle(serialPortHandle);
	serialPortHandle = INVALID_HANDLE_VALUE;
	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);

	return (retVal == 0) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_bytesAvailable(JNIEnv *env, jobject obj)
{
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	COMSTAT commInfo;
	DWORD numBytesAvailable;

	if (!ClearCommError(serialPortHandle, NULL, &commInfo))
		return -1;
	numBytesAvailable = commInfo.cbInQue;
	
	return (jint)numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
    OVERLAPPED overlappedStruct = {0};
    overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    jbyte *readBuffer = env->GetByteArrayElements(buffer, 0);
    DWORD numBytesRead = 0;
    BOOL result;

    // Read from serial port
    if ((result = ReadFile(serialPortHandle, readBuffer, bytesToRead, &numBytesRead, &overlappedStruct)) != FALSE)	// Immediately successful
        env->ReleaseByteArrayElements(buffer, readBuffer, 0);
    else if (GetLastError() != ERROR_IO_PENDING)		// Problem occurred
    {
    	// Problem reading, close port
    	CloseHandle(serialPortHandle);
    	serialPortHandle = INVALID_HANDLE_VALUE;
    	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
    	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
    }
    else		// Pending read operation
    {
    	switch (WaitForSingleObject(overlappedStruct.hEvent, INFINITE))
    	{
    		case WAIT_OBJECT_0:
    			if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesRead, FALSE)) != FALSE)
    				env->ReleaseByteArrayElements(buffer, readBuffer, 0);
    			else
    			{
    				// Problem reading, close port
    				CloseHandle(serialPortHandle);
    				serialPortHandle = INVALID_HANDLE_VALUE;
    				env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
    				env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
    			}
    			break;
    		default:
    			// Problem reading, close port
    			CloseHandle(serialPortHandle);
    			serialPortHandle = INVALID_HANDLE_VALUE;
    			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
    			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
    			break;
    	}
    }

    // Return number of bytes read if successful
    CloseHandle(overlappedStruct.hEvent);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	HANDLE serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	OVERLAPPED overlappedStruct = {0};
	overlappedStruct.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	jbyte *writeBuffer = env->GetByteArrayElements(buffer, 0);
	DWORD numBytesWritten = 0;
	BOOL result;

	// Write to port
	if ((result = WriteFile(serialPortHandle, writeBuffer, bytesToWrite, &numBytesWritten, &overlappedStruct)) == FALSE)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			// Problem writing, close port
			CloseHandle(serialPortHandle);
			serialPortHandle = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
		else
		{
			switch (WaitForSingleObject(overlappedStruct.hEvent, INFINITE))
			{
				case WAIT_OBJECT_0:
					if ((result = GetOverlappedResult(serialPortHandle, &overlappedStruct, &numBytesWritten, FALSE)) == FALSE)
			    	{
			    		// Problem reading, close port
			    		CloseHandle(serialPortHandle);
			    		serialPortHandle = INVALID_HANDLE_VALUE;
			    		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
			    		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
			    	}
			    	break;
			    default:
			    	// Problem reading, close port
			    	CloseHandle(serialPortHandle);
			    	serialPortHandle = INVALID_HANDLE_VALUE;
			    	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
			    	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
			    	break;
			}
		}
	}

	// Return number of bytes written if successful
	CloseHandle(overlappedStruct.hEvent);
	return (result == TRUE) ? numBytesWritten : -1;
}

#endif
