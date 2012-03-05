package j.extensions.comm;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class SerialComm
{
	// Static initializer loads correct native library for this machine
	static
	{
		String OS = System.getProperty("os.name").toLowerCase();
		String copyFromPath = "", fileName = "";
	
		// Determine Operating System and architecture
		if (OS.indexOf("win") >= 0)
		{
			if ((System.getProperty("os.arch").indexOf("64") >= 0) ||
					(new File("C:\\Program Files (x86)").exists()))
				copyFromPath = "Win64";
			else
				copyFromPath = "Win32";
			fileName = "SerialComm.dll";
		}
		else if (OS.indexOf("mac") >= 0)
		{
			String resultString = "";
			try
			{
				InputStream inStream = Runtime.getRuntime().exec("getconf LONG_BIT").getInputStream();
				resultString += (char)inStream.read();
				resultString += (char)inStream.read();
				inStream.close();
			} catch (Exception e) {}
			
			if ((System.getProperty("os.arch").indexOf("64") >= 0) ||
					(resultString.indexOf("64") >= 0))
				copyFromPath = "OSX64";
			else
				copyFromPath = "OSX32";
			fileName = "libSerialComm.jnilib";
		}
		else if ((OS.indexOf("nix") >= 0) || (OS.indexOf("nux") >= 0))
		{
			String resultString = "";
			try
			{
				InputStream inStream = Runtime.getRuntime().exec("getconf LONG_BIT").getInputStream();
				resultString += (char)inStream.read();
				resultString += (char)inStream.read();
				inStream.close();
			} catch (Exception e) {}
			
			if ((System.getProperty("os.arch").indexOf("64") >= 0) ||
					(resultString.indexOf("64") >= 0))
				copyFromPath = "Linux64";
			else
				copyFromPath = "Linux32";
			fileName = "";
		}
		else
		{
			System.err.println("This operating system is not supported by the SerialComm library.");
			System.exit(-1);
		}
		
		// Delete any native libraries to ensure the correct library is loaded each time
		File tempNativeLibrary = new File("SerialComm.dll");
		if (tempNativeLibrary.exists())
			tempNativeLibrary.delete();
		tempNativeLibrary = new File("libSerialComm.jnilib");
		if (tempNativeLibrary.exists())
			tempNativeLibrary.delete();
		
		// Get path of native library and copy file to working directory
		tempNativeLibrary = new File(fileName);
		tempNativeLibrary.deleteOnExit();
		try
		{
			InputStream fileContents = SerialComm.class.getResourceAsStream("/" + copyFromPath + "/" + fileName);
			FileOutputStream destinationFileContents = new FileOutputStream(tempNativeLibrary);
			byte transferBuffer[] = new byte[4096];
			int numBytesRead;
			
			while ((numBytesRead = fileContents.read(transferBuffer)) > 0)
				destinationFileContents.write(transferBuffer, 0, numBytesRead);
			
			fileContents.close();
			destinationFileContents.close();
		}
		catch (Exception e) { e.printStackTrace(); }
		
		// Load native library
		System.loadLibrary("SerialComm");
	}
	
	// Returns a list of all availble serial ports on this machine
	static public native SerialComm[] getCommPorts();
	
	// Parity Values
	static final public int NO_PARITY = 0;
	static final public int ODD_PARITY = 1;
	static final public int EVEN_PARITY = 2;
	static final public int MARK_PARITY = 3;
	static final public int SPACE_PARITY = 4;

	// Number of Stop Bits
	static final public int ONE_SB = 1;
	static final public int ONE_POINT_FIVE_SB = 2;
	static final public int TWO_SB = 3;
	
	// Serial Port Parameters
	private volatile int baudRate = 9600, byteSize = 8, stopBits = ONE_SB, parity = NO_PARITY;
	private volatile int readTimeout = 0, writeTimeout = 0;
	private volatile SerialCommInputStream inputStream = null;
	private volatile SerialCommOutputStream outputStream = null;
	private volatile String portString, comPort;
	private volatile long portHandle = -1l;
	private volatile boolean isOpened = false;
	
	// Serial Port Setup Methods
	public final native boolean openPort();								// Opens this serial port for reading/writing
	public final native boolean closePort();							// Closes this serial port
	private final native boolean configPort();							// Changes/sets serial port parameters as defined by this class
	private final native boolean configTimeouts();						// Changes/sets serial port timeouts as defined by this class
	
	// Serial Port Read/Write Methods
	public final native int readBytes(byte[] buffer, long bytesToRead);
	public final native int writeBytes(byte[] buffer, long bytesToWrite);
	
	// Java methods
	public SerialComm() {}
	public final InputStream getInputStream()
	{
		if ((inputStream == null) && isOpened)
			inputStream = new SerialCommInputStream();
		return inputStream;
	}
	public final OutputStream getOutputStream()
	{
		if ((outputStream == null) && isOpened)
			outputStream = new SerialCommOutputStream();
		return outputStream;
	}
	
	// Setters
	public final void setComPortParameters(int newBaudRate, int newByteSize, int newStopBits, int newParity)
	{
		baudRate = newBaudRate;
		byteSize = newByteSize;
		stopBits = newStopBits;
		parity = newParity;
		configPort();
	}
	public final void setComPortTimeouts(int newReadTimeout, int newWriteTimeout)
	{
		readTimeout = newReadTimeout;
		writeTimeout = newWriteTimeout;
		configTimeouts();
	}
	public final void setBaudRate(int newBaudRate) { baudRate = newBaudRate; configPort(); }
	public final void setByteSize(int newByteSize) { byteSize = newByteSize; configPort(); }
	public final void setNumStopBits(int newStopBits) { stopBits = newStopBits; configPort(); }
	public final void setParity(int newParity) { parity = newParity; configPort(); }
	
	// Getters
	public final String getDescriptivePortName() { return portString.trim(); }
	public final String getSystemPortName() { return comPort.substring(comPort.lastIndexOf('\\')+1); }
	public final int getBaudRate() { return baudRate; }
	public final int getByteSize() { return byteSize; }
	public final int getNumStopBits() { return stopBits; }
	public final int getParity() { return parity; }
	public final int getReadTimeout() { return readTimeout; }
	public final int getWriteTimeout() { return writeTimeout; }
	
	private final void fixAndSetComPort(String comPortName)
	{
		comPort = "\\\\.\\";
		if (comPortName.charAt(0) != '\\')
			comPort += comPortName;
		else
			comPort = comPortName;
	}
	
	private final void fixAndSetPortString(String portStringName)
	{
		portString = portStringName.substring(portStringName.lastIndexOf('\\') + 1);
	}
	
	// InputStream interface class
	private final class SerialCommInputStream extends InputStream
	{
		@Override
		public final int available() throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			return 1;
		}
		
		@Override
		public final int read() throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[1];
			return (readBytes(buffer, 1l) > 0) ? buffer[0] : -1;
		}
		
		@Override
		public final int read(byte[] b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			return read(b, 0, b.length);
			
		}
		
		@Override
		public final int read(byte[] b, int off, int len) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			if (len == 0)
				return 0;
			
			byte[] buffer = new byte[len];
			int numRead = readBytes(buffer, len);
			if (numRead > 0)
				System.arraycopy(buffer, 0, b, off, numRead);
			
			return numRead;
		}
		
		@Override
		public final long skip(long n) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[(int)n];
			return readBytes(buffer, n);
		}
	}
	
	// OutputStream interface class
	private final class SerialCommOutputStream extends OutputStream
	{
		@Override
		public final void write(int b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[1];
			buffer[0] = (byte)(b & 0x000000FF);
			if (writeBytes(buffer, 1l) < 0)
				throw new IOException("This port appears to have been shutdown or disconnected.");
		}
		
		@Override
		public final void write(byte[] b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			write(b, 0, b.length);
		}
		
		@Override
		public final void write(byte[] b, int off, int len) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[len];
			System.arraycopy(b, off, buffer, 0, len);
			if (writeBytes(buffer, len) < 0)
				throw new IOException("This port appears to have been shutdown or disconnected.");
		}
	}
	
	static public void main(String[] args)
	{
		SerialComm[] ports = SerialComm.getCommPorts();
		System.out.println("Ports:");
		for (int i = 0; i < ports.length; ++i)
			System.out.println("   " + ports[i].getSystemPortName() + ": " + ports[i].getDescriptivePortName());
		SerialComm ubxPort = ports[0];
		
		byte[] readBuffer = new byte[2048];
		System.out.println("Opening " + ubxPort.getDescriptivePortName() + ": " + ubxPort.openPort());
		ubxPort.setComPortTimeouts(50, 0);
		InputStream in = ubxPort.getInputStream();
		try
		{
			for (int i = 0; i < 3; ++i)
			{
				System.out.println("\nReading #" + i);
				int numRead = ubxPort.readBytes(readBuffer, readBuffer.length);
				System.out.println("Read " + numRead + " bytes.");
				
				//for (int j = 0; j < 1000; ++j)
					//System.out.print((char)in.read());
			}
			in.close();
		}catch (Exception e) {}
		
		ubxPort.closePort();
	}
}
