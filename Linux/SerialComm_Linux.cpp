/*
 * SerialComm.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: hedgecrw
 */

#ifdef __linux__
#ifndef CMSPAR
#define CMSPAR 010000000000
#endif
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <fcntl.h>
#include <dirent.h>
#include <cerrno>
#include <unistd.h>
#include <termios.h>
#include "../j_extensions_comm_SerialComm.h"

JNIEXPORT jobjectArray JNICALL Java_j_extensions_comm_SerialComm_getCommPorts(JNIEnv *env, jclass serialCommClass)
{
	DIR *serialPortIterator;
	struct dirent *serialPortEntry;
	int serialPort;
	int numValues = -2, numChars, index = 0;
	char portString[1024], comPort[1024], pathBase[21] = {"/dev/serial/by-path/"};

	// Get relevant SerialComm methods and IDs
	jmethodID serialCommConstructor = env->GetMethodID(serialCommClass, "<init>", "()V");
	jfieldID portStringID = env->GetFieldID(serialCommClass, "portString", "Ljava/lang/String;");
	jfieldID comPortID = env->GetFieldID(serialCommClass, "comPort", "Ljava/lang/String;");

	// Enumerate serial ports on machine
	if ((serialPortIterator = opendir(pathBase)) == NULL)
		return NULL;
	while (readdir(serialPortIterator) != NULL) ++numValues;
	rewinddir(serialPortIterator);
	jobjectArray arrayObject = env->NewObjectArray(numValues, serialCommClass, 0);
	while ((serialPortEntry = readdir(serialPortIterator)) != NULL)
	{
		// Get serial COM value
		strcpy(portString, pathBase);
		strcpy(portString+20, serialPortEntry->d_name);
		if ((numChars = readlink(portString, comPort, sizeof(comPort))) == -1)
			continue;
		comPort[numChars] = '\0';

		// Get port name
		strcpy(portString, strrchr(comPort, '/') + 1);
		strcpy(comPort, "/dev/");
		strcpy(comPort+5, portString);

		// Create new SerialComm object containing the enumerated values
		jobject serialCommObject = env->NewObject(serialCommClass, serialCommConstructor);
		env->SetObjectField(serialCommObject, portStringID, env->NewStringUTF(portString));
		env->SetObjectField(serialCommObject, comPortID, env->NewStringUTF(comPort));

		// Add new SerialComm object to array
		env->SetObjectArrayElement(arrayObject, index++, serialCommObject);
	}
	closedir(serialPortIterator);

	return arrayObject;
}

JNIEXPORT jboolean JNICALL Java_j_extensions_comm_SerialComm_openPort(JNIEnv *env, jobject obj)
{
	int fdSerial;
	jstring portNameJString = (jstring)env->GetObjectField(obj, env->GetFieldID(env->GetObjectClass(obj), "comPort", "Ljava/lang/String;"));
	const char *portName = env->GetStringUTFChars(portNameJString, NULL);

	// Try to open existing serial port with read/write access
	if ((fdSerial = open(portName, O_RDWR | O_NOCTTY | O_NDELAY)) > 0)
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
	struct serial_struct serialInfo;
	jclass serialCommClass = env->GetObjectClass(obj);
	int portFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));

	// Set raw-mode to allow the use of tcsetattr() and ioctl()
	fcntl(portFD, F_SETFL, 0);
	cfmakeraw(&options);

	// Get port parameters from Java class
	int baudRate = env->GetIntField(obj, env->GetFieldID(serialCommClass, "baudRate", "I"));
	int byteSizeInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "dataBits", "I"));
	int stopBitsInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "stopBits", "I"));
	int parityInt = env->GetIntField(obj, env->GetFieldID(serialCommClass, "parity", "I"));
	tcflag_t byteSize = (byteSizeInt == 5) ? CS5 : (byteSizeInt == 6) ? CS6 : (byteSizeInt == 7) ? CS7 : CS8;
	tcflag_t stopBits = ((stopBitsInt == j_extensions_comm_SerialComm_ONE_STOP_BIT) || (stopBitsInt == j_extensions_comm_SerialComm_ONE_POINT_FIVE_STOP_BITS)) ? 0 : CSTOPB;
	tcflag_t parity = (parityInt == j_extensions_comm_SerialComm_NO_PARITY) ? 0 : (parityInt == j_extensions_comm_SerialComm_ODD_PARITY) ? (PARENB | PARODD) : (parityInt == j_extensions_comm_SerialComm_EVEN_PARITY) ? PARENB : (parityInt == j_extensions_comm_SerialComm_MARK_PARITY) ? (PARENB | CMSPAR | PARODD) : (PARENB | CMSPAR);
	int flowControl = env->GetIntField(obj, env->GetFieldID(serialCommClass, "flowControl", "I"));
	tcflag_t XonXoffInEnabled = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_XONXOFF_IN_ENABLED) > 0) ? IXOFF : 0;
	tcflag_t XonXoffOutEnabled = ((flowControl & j_extensions_comm_SerialComm_FLOW_CONTROL_XONXOFF_OUT_ENABLED) > 0) ? IXON : 0;

	// Retrieve existing port configuration
	tcgetattr(portFD, &options);

	// Set updated port parameters
	options.c_cflag = (B38400 | byteSize | stopBits | parity | CLOCAL | CREAD);
	if (parityInt == j_extensions_comm_SerialComm_SPACE_PARITY)
		options.c_cflag &= ~PARODD;
	options.c_iflag = XonXoffInEnabled | XonXoffOutEnabled | ((parityInt > 0) ? (INPCK | ISTRIP) : IGNPAR);
	options.c_oflag = 0;
	options.c_lflag = 0;

	// Apply changes
	tcsetattr(portFD, TCSAFLUSH, &options);
	ioctl(portFD, TIOCEXCL);				// Block non-root users from using this port

	// Allow custom baud rate
	ioctl(portFD, TIOCGSERIAL, &serialInfo);
	serialInfo.flags = ASYNC_SPD_CUST | ASYNC_LOW_LATENCY;
	serialInfo.custom_divisor = serialInfo.baud_base / baudRate;
	ioctl(portFD, TIOCSSERIAL, &serialInfo);
	return JNI_TRUE;
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
		options.c_cc[VTIME] = readTimeout / 100;
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

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_available(JNIEnv *env, jobject obj)
{
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	int numBytesAvailable;

    ioctl(serialPortFD, FIONREAD, &numBytesAvailable);
	
	return numBytesAvailable;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_readBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToRead)
{
	// Get port handle and read timeout from Java class
	jbyte *readBuffer = env->GetByteArrayElements(buffer, 0);
	jclass serialCommClass = env->GetObjectClass(obj);
	int readTimeout = env->GetIntField(obj, env->GetFieldID(serialCommClass, "readTimeout", "I"));
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(serialCommClass, "portHandle", "J"));
	int numBytesRead, bytesRemaining = bytesToRead, index = 0;

	// No timeout specified, don't return until we have completely finished the read
	if (readTimeout == 0)
	{
		// While there are more bytes we are supposed to read
		while (bytesRemaining > 0)
		{
			if ((numBytesRead = read(serialPortFD, readBuffer+index, bytesRemaining)) == -1)
			{
				// Problem reading, close port
				close(serialPortFD);
				serialPortFD = -1;
				env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
				env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
				break;
			}

			// Fix index variables
			index += numBytesRead;
			bytesRemaining -= numBytesRead;
		}

		// Set return values
		env->ReleaseByteArrayElements(buffer, readBuffer, 0);
		numBytesRead = bytesToRead;
	}
	else		// Timeouts specified
	{
		// Read from port
		if ((numBytesRead = read(serialPortFD, readBuffer, bytesToRead)) > -1)
			env->ReleaseByteArrayElements(buffer, readBuffer, 0);
		else
		{
			// Problem reading, close port
			close(serialPortFD);
			serialPortFD = -1;
			env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
			env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
		}
	}

	// Return number of bytes read if successful
	return numBytesRead;
}

JNIEXPORT jint JNICALL Java_j_extensions_comm_SerialComm_writeBytes(JNIEnv *env, jobject obj, jbyteArray buffer, jlong bytesToWrite)
{
	jbyte *writeBuffer = env->GetByteArrayElements(buffer, 0);
	int serialPortFD = (int)env->GetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"));
	int numBytesWritten;

	// Write to port
	if ((numBytesWritten = write(serialPortFD, writeBuffer, bytesToWrite)) == -1)
	{
		// Problem writing, close port
		close(serialPortFD);
		serialPortFD = -1;
		env->SetLongField(obj, env->GetFieldID(env->GetObjectClass(obj), "portHandle", "J"), -1l);
		env->SetBooleanField(obj, env->GetFieldID(env->GetObjectClass(obj), "isOpened", "Z"), JNI_FALSE);
	}

	// Return number of bytes written if successful
	return numBytesWritten;
}

#endif
