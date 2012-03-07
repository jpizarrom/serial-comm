/*
 * SerialComm.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: hedgecrw
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
	jmethodID serialCommSetPortStringMethod = env->GetMethodID(serialCommClass, "fixAndSetPortString", "(Ljava/lang/String;)V");
	jmethodID serialCommSetComPortMethod = env->GetMethodID(serialCommClass, "fixAndSetComPort", "(Ljava/lang/String;)V");

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
		env->CallVoidMethod(serialCommObject, serialCommSetPortStringMethod, env->NewStringUTF(valueName));
		env->CallVoidMethod(serialCommObject, serialCommSetComPortMethod, env->NewStringUTF((char*)comPort));

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
	}

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	HANDLE hSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((hSerial = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0)) != INVALID_HANDLE_VALUE)
	{
		// Set port handle in Java structure
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)hSerial);

		// Configure the port parameters and timeouts
		if (Java_j_extensions_comm_SerialComm_configPort(env, obj) && Java_j_extensions_comm_SerialComm_configTimeouts(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
			CloseHandle(hSerial);
			hSerial = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (hSerial == INVALID_HANDLE_VALUE) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configPort(JNIEnv *env, jobject obj)
{
	DCB dcbSerialParams = {0};
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	jclass serialCommClass = env->GetObjectClass(obj);

	// Get port parameters from Java class
	HANDLE serialHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	DWORD baudRate = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	BYTE byteSize = (BYTE)env->GetIntField(obj, env->GetFieldID(serialCommClass, "byteSize", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	BYTE stopBits = (stopBitsInt == j_extensions_comm_SerialComm_ONE_STOP_BIT) ? ONESTOPBIT : (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_STOP_BITS) ? ONE5STOPBITS : TWOSTOPBITS;
	BYTE parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? NOPARITY : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? ODDPARITY : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? EVENPARITY : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? MARKPARITY : SPACEPARITY;
	BOOL isParity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? FALSE : TRUE;

	// Retrieve existing port configuration
	if (!GetCommState(serialHandle, &dcbSerialParams))
		return JNI_FALSE;

	// Set updated port parameters
	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = byteSize;
	dcbSerialParams.StopBits = stopBits;
	dcbSerialParams.Parity = parity;
	dcbSerialParams.fParity = isParity;
	dcbSerialParams.fAbortOnError = FALSE;
	dcbSerialParams.fOutxDsrFlow = FALSE;
	dcbSerialParams.fDsrSensitivity = FALSE;
	dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;

	// Apply changes
	return SetCommState(serialHandle, &dcbSerialParams);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configTimeouts(JNIEnv *env, jobject obj)
{
	// Get port timeouts from Java class
	COMMTIMEOUTS timeouts = {0};
	jclass serialCommClass = env->GetObjectClass(obj);
	HANDLE serialHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	DWORD readTimeout = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));
	DWORD writeTimeout = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "writeTimeout", "I"));

	// Set updated port timeouts
	timeouts.ReadIntervalTimeout = 0;
	timeouts.ReadTotalTimeoutConstant = readTimeout;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = writeTimeout;
	timeouts.WriteTotalTimeoutMultiplier = 0;

	// Apply changes
	return SetCommTimeouts(serialHandle, &timeouts);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_closePort(JNIEnv *env, jobject obj)
{
	// Close port
	HANDLE portHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	BOOL retVal = CloseHandle(portHandle);
	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);

	return (retVal == 0) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	signed char *readBuffer = (signed char*)malloc(bytesToRead);
	DWORD numBytesRead = 0;
	BOOL result;

	// Cache serial handle so we don't have to fetch it for every read/write
	HANDLE serialPortHandle = INVALID_HANDLE_VALUE;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	// Read from port
	if ((result = ReadFile(serialPortHandle, readBuffer, bytesToRead, &numBytesRead, NULL)) != FALSE)
		env->SetByteArrayRegion(buffer, 0, numBytesRead, readBuffer);
	else
	{
		// Problem reading, close port
		CloseHandle(serialPortHandle);
		serialPortHandle = INVALID_HANDLE_VALUE;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes read if successful
	free(readBuffer);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	signed char *writeBuffer = (signed char*)malloc(bytesToWrite);
	DWORD numBytesWritten = 0;
	BOOL result;

	// Cache serial handle so we don't have to fetch it for every read/write
	HANDLE serialPortHandle = INVALID_HANDLE_VALUE;
	if (serialPortHandle == INVALID_HANDLE_VALUE)
		serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	// Write to port
	env->GetByteArrayRegion(buffer, 0, bytesToWrite, writeBuffer);
	if ((result = WriteFile(serialPortHandle, writeBuffer, bytesToWrite, &numBytesWritten, NULL)) == FALSE)
	{
		// Problem writing, close port
		CloseHandle(serialPortHandle);
		serialPortHandle = INVALID_HANDLE_VALUE;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)INVALID_HANDLE_VALUE);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes written if successful
	free(writeBuffer);
	return (result == TRUE) ? numBytesWritten : -1;
}

#endif
