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
#include <cstdlib>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "../j_extensions_comm_SerialComm.h"

JNIEXPORT jobjectArray JNICALL Java_j_extensions_comm_SerialComm_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	io_object_t serialPort;
	io_iterator_t serialPortIterator;
	int numValues = 0;
	char portString[1024], comPort[1024];

	// Get relevant SerialComm methods and IDs
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");
	jfieldID portStringID = env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;");
	jfieldID comPortID = env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;");

	// Enumerate serial ports on machine
	IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &serialPortIterator);
	while (IOIteratorNext(serialPortIterator)) ++numValues;
	IOIteratorReset(serialPortIterator);
	jobjectArray arrayObject = env->NewObjectArray(numValues, serialCommClass, 0);
	for (int i = 0; i < numValues; ++i)
	{
		// Get serial port name and COM value
		serialPort = IOIteratorNext(serialPortIterator);
		CFStringRef portStringRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOTTYDeviceKey), kCFAllocatorDefault, 0);
		CFStringRef comPortRef = (CFStringRef)IORegistryEntryCreateCFProperty(serialPort, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
		CFStringGetCString(portStringRef, portString, sizeof(portString), kCFStringEncodingUTF8);
		CFStringGetCString(comPortRef, comPort, sizeof(comPort), kCFStringEncodingUTF8);
		CFRelease(portStringRef);
		CFRelease(comPortRef);

		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		env->SetObjectField(serialCommObject, portStringID, env->NewStringUTF(portString));
		env->SetObjectField(serialCommObject, comPortID, env->NewStringUTF(comPort));

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, i, serialCommObject);
		IOObjectRelease(serialPort);
	}
	IOObjectRelease(serialPortIterator);

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	int fdSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((fdSerial = open(portName, O_RDWR | O_NOCTTY)) > 0)
	{
		// Set port handle in Java structure
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), fdSerial);

		// Configure the port parameters and timeouts
		if (Java_j_extensions_comm_SerialComm_configPort(env, obj) && Java_j_extensions_comm_SerialComm_configTimeouts(env, obj))
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_TRUE);
		else
		{
			// Close the port if there was a problem setting the parameters
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
	struct termios options;
	jclass serialCommClass = env->GetObjectClass(obj);
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));

	// Set raw-mode to allow the use of tcsetattr() and ioctl()
	fcntl(portFD, F_SETFL, 0);
	cfmakeraw(&options);

	// Get port parameters from Java class
	speed_t baudRate = env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	int byteSizeInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "byteSize", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == j_extensions_comm_SerialComm_ONE_STOP_BIT) || (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_STOP_BITS)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? 0 : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? PARENB : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);

	// Retrieve existing port configuration
	tcgetattr(portFD, &options);

	// Set updated port parameters
	options.c_cflag = (B9600 | byteSize | stopBits | parity | CLOCAL | CREAD);
	if (parityInt == j_extensions_comm_SerialComm_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag = (parity & PARENB > 0) ? (INPCK | ISTRIP) : 0;
	options.c_oflag = 0;
	options.c_lflag = 0;

	// Apply changes
	tcsetattr(portFD, TCSAFLUSH, &options);
	ioctl(portFD, TIOCEXCL);				// Block non-root users from using this port
	return (ioctl(portFD, IOSSIOSPEED, &baudRate) == -1) ? JNI_FALSE : JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_configTimeouts(JNIEnv *env, jobject obj)
{
	// Get port timeouts from Java class
	jclass serialCommClass = env->GetObjectClass(obj);
	int serialFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	int readTimeout = env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));

	// Retrieve existing port configuration
	struct termios options;
	tcgetattr(serialFD, &options);

	// Set updated port timeouts
	if (readTimeout == 0)
	{
		options.c_cc[VMIN] = 1;
		options.c_cc[VTIME] = 0;
	}
	else
	{
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = readTimeout;
	}

	// Apply changes
	return (tcsetattr(serialFD, TCSAFLUSH, &options) == 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_closePort(JNIEnv *env, jobject obj)
{
	// Close port
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

	// Cache serial handle so we don't have to fetch it for every read/write
	static int serialPortFD = -1;
	if (serialPortFD == -1)
		serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	// Read from port
	if ((numBytesRead = read(serialPortFD, readBuffer, bytesToRead)) > -1)
		env->SetByteArrayRegion(buffer, 0, numBytesRead, readBuffer);
	else
	{
		// Problem reading, close port
		close(serialPortFD);
		serialPortFD = -1;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes read if successful
	free(readBuffer);
	return numBytesRead;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	signed char *writeBuffer = (signed char*)malloc(bytesToWrite);
	int numBytesWritten;

	// Cache serial handle so we don't have to fetch it for every read/write
	static int serialPortFD = -1;
	if (serialPortFD == -1)
		serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));

	// Write to port
	env->GetByteArrayRegion(buffer, 0, bytesToWrite, writeBuffer);
	if ((numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite)) == -1)
	{
		// Problem writing, close port
		close(serialPortFD);
		serialPortFD = -1;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes written if successful
	free(writeBuffer);
	return numBytesWritten;
}

#endif

