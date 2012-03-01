/*
 * SerialComm.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: hedgecrw
 */

#ifdef __APPLE__
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#ifndef IOSSIOSPEED
#define IOSSIOSPEED _IOW('T', 2, speed_t)
#endif
#include <cstdlib>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "../j_extensions_comm_SerialComm.h"

JNIEXPORT jobject JNICALL Java_j_extensions_comm_SerialComm_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	//DWORD numValues, valueLength, comPortLength;
	//HKEY keyHandle;
	//CHAR valueName[128];
	//BYTE comPort[128];

	// Create new ArrayList of SerialComm objects
	jclass arrayListClass = env->FindClass("Ljava/util/ArrayList;");
	jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "(I)V");
	jmethodID arrayListAddMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");
	jmethodID serialCommSetPortNameMethod = env->GetMethodID(serialCommClass, "fixAndSetComPort", "(Ljava/lang/String;)V");
	jobject arrayListObject = env->NewObject(arrayListClass, arrayListConstructor, 0);

	// Enumerate serial ports on machine
	/*RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_QUERY_VALUE, &keyHandle);
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
	}*/

	return arrayListObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	int fdSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	if ((fdSerial = open(portName, O_RDWR | O_NOCTTY)) > 0)
	{
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), fdSerial);

		if (Java_j_extensions_comm_SerialComm_configPort(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			close(fdSerial);
			fdSerial = -1;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	env->ReleaseStringUTFChars(portNameJString, portName);
	return (fdSerial == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configPort(JNIEnv *env, jobject obj)
{
	jclass serialCommClass = env->GetObjectClass(obj);
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	speed_t baudRate = env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	int byteSizeInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "byteSize", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == j_extensions_comm_SerialComm_ONE_SB) || (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_SB)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? 0 : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? PARENB : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);

	struct termios options;
	fcntl(portFD, F_SETFL, 0);
	tcgetattr(portFD, &options);
	options.c_cflag = (B9600 | byteSize | stopBits | parity | CLOCAL | CREAD);
	if (parityInt == j_extensions_comm_SerialComm_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag = (parity & PARENB > 0) ? (INPCK | ISTRIP) : 0;
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_cc[VMIN] = 1;
	options.c_cc[VTIME] = 0;
	tcsetattr(portFD, TCSAFLUSH, &options);

	return (ioctl(portFD, IOSSIOSPEED, &baudRate) == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_closePort(JNIEnv *env, jobject obj)
{
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	close(portFD);
	env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
	env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	signed char *readBuffer = (signed char*)malloc(bytesToRead);
	int numBytesRead;

	static int serialPortFD = -1;
	if (serialPortFD == -1)
		serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	if ((numBytesRead = read(serialPortFD, readBuffer, bytesToRead)) > -1)
		env->SetByteArrayRegion(buffer, 0, numBytesRead, readBuffer);
	else
	{
		close(serialPortFD);
		serialPortFD = -1;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	free(readBuffer);
	return numBytesRead;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	signed char *writeBuffer = (signed char*)malloc(bytesToWrite);
	int numBytesWritten;

	static int serialPortFD = -1;
	if (serialPortFD == -1)
		serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	env->GetByteArrayRegion(buffer, 0, bytesToWrite, writeBuffer);
	if ((numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite)) == -1)
	{
		close(serialPortFD);
		serialPortFD = -1;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	free(writeBuffer);
	return numBytesWritten;
}

#endif

