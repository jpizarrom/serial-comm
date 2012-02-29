/*
 * SerialComm.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: hedgecrw
 */

#ifdef __APPLE__
#include <cstdlib>
#include <fcntl.h>
#include <termios.h>
#include "../j_extensions_comm_SerialComm.h"

JNIEXPORT jobject JNICALL Java_j_extensions_comm_SerialComm_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	DWORD numValues, valueLength, comPortLength;
	HKEY keyHandle;
	CHAR valueName[128];
	BYTE comPort[128];

	// Create new ArrayList of SerialComm objects
	jclass arrayListClass = env->FindClass("Ljava/util/ArrayList;");
	jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "(I)V");
	jmethodID arrayListAddMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");
	jmethodID serialCommSetPortNameMethod = env->GetMethodID(serialCommClass, "fixAndSetComPort", "(Ljava/lang/String;)V");
	jobject arrayListObject = env->NewObject(arrayListClass, arrayListConstructor, 0);

	// Enumerate serial ports on machine
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle);
	RegQueryInfoKey(keyHandle, NULL, NULL, NULL, NULL, NULL, NULL, &numValues, NULL, NULL, NULL, NULL);
	for (DWORD i = 0; i < numValues; ++i)
	{
		// Get serial port name and COM value
		valueLength = comPortLength = 128;
		RegEnumValue(keyHandle, i, valueName, &valueLength, NULL, NULL, comPort, &comPortLength);

		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		env->SetObjectField(serialCommObject, env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;"), env->NewStringUTF(valueName));
		env->CallVoidMethod(serialCommObject, serialCommSetPortNameMethod, env->NewStringUTF((char*)comPort));

		// Add new SerialComm object to array list
		env->CallBooleanMethod(arrayListObject, arrayListAddMethod, serialCommObject);
	}

	return arrayListObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	HANDLE hSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	if ((hSerial = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE)
	{
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), (jlong)hSerial);

		if (Java_j_extensions_comm_SerialComm_configPort(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			CloseHandle(hSerial);
			hSerial = INVALID_HANDLE_VALUE;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
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

	HANDLE serialHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	DWORD baudRate = (DWORD)env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	BYTE byteSize = (BYTE)env->GetIntField(obj, env->GetFieldID(serialCommClass, "byteSize", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	BYTE stopBits = (stopBitsInt == j_extensions_comm_SerialComm_ONE_SB) ? ONESTOPBIT : (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_SB) ? ONE5STOPBITS : TWOSTOPBITS;
	BYTE parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? NOPARITY : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? ODDPARITY : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? EVENPARITY : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? MARKPARITY : SPACEPARITY;

	if (!GetCommState(serialHandle, &dcbSerialParams))
		return false;

	dcbSerialParams.BaudRate = baudRate;
	dcbSerialParams.ByteSize = byteSize;
	dcbSerialParams.StopBits = stopBits;
	dcbSerialParams.Parity = parity;
	dcbSerialParams.fOutX = FALSE;
	dcbSerialParams.fInX = FALSE;

	return SetCommState(serialHandle, &dcbSerialParams);
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_closePort(JNIEnv *env, jobject obj)
{
	HANDLE portHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	CloseHandle(portHandle);
	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	signed char *readBuffer = (signed char*)malloc(bytesToRead);
	DWORD numBytesRead = 0;
	BOOL result;

	static HANDLE serialPortHandle = NULL;
	if (serialPortHandle == NULL)
		serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	if ((result = ReadFile(serialPortHandle, readBuffer, bytesToRead, &numBytesRead, NULL)) != FALSE)
		env->SetByteArrayRegion(buffer, 0, numBytesRead, readBuffer);
	else
	{
		CloseHandle(serialPortHandle);
		serialPortHandle = NULL;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	free(readBuffer);
	return (result == TRUE) ? numBytesRead : -1;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	signed char *writeBuffer = (signed char*)malloc(bytesToWrite);
	DWORD numBytesWritten = 0;
	BOOL result;

	static HANDLE serialPortHandle = NULL;
	if (serialPortHandle == NULL)
		serialPortHandle = (HANDLE)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	env->GetByteArrayRegion(buffer, 0, bytesToWrite, writeBuffer);
	if ((result = WriteFile(serialPortHandle, writeBuffer, bytesToWrite, &numBytesWritten, NULL)) == FALSE)
	{
		CloseHandle(serialPortHandle);
		serialPortHandle = NULL;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	free(writeBuffer);
	return (result == TRUE) ? numBytesWritten : -1;
}

#endif

