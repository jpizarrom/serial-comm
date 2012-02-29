package j.extensions.comm;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;

public class SerialComm
{
	static { System.loadLibrary("SerialComm"); }
	
	// Returns a list of all availble serial ports on this machine
	static public native ArrayList<SerialComm> getCommPorts();
	
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
	private int baudRate = 9600, byteSize = 8, stopBits = ONE_SB, parity = NO_PARITY;
	private SerialCommInputStream inputStream = null;
	private SerialCommOutputStream outputStream = null;
	private String portString, comPort;
	private long portHandle = -1l;
	private boolean isOpened = false;
	
	// Serial Port Setup Methods
	public native boolean openPort();								// Opens this serial port for reading/writing
	public native boolean closePort();								// Closes this serial port
	private native boolean configPort();							// Changes/sets serial port parameters as defined by this class
	
	// Serial Port Read/Write Methods
	public native int readBytes(byte[] buffer, long bytesToRead);
	public native int writeBytes(byte[] buffer, long bytesToWrite);
	
	// Java methods
	public SerialComm() {}
	public InputStream getInputStream()
	{
		if ((inputStream == null) && isOpened)
			inputStream = new SerialCommInputStream();
		return inputStream;
	}
	public OutputStream getOutputStream()
	{
		if ((outputStream == null) && isOpened)
			outputStream = new SerialCommOutputStream();
		return outputStream;
	}
	
	// Setters
	public void setComPortParameters(int newBaudRate, int newByteSize, int newStopBits, int newParity)
	{
		baudRate = newBaudRate;
		byteSize = newByteSize;
		stopBits = newStopBits;
		parity = newParity;
		configPort();
	}
	public void setBaudRate(int newBaudRate) { baudRate = newBaudRate; configPort(); }
	public void setByteSize(int newByteSize) { byteSize = newByteSize; configPort(); }
	public void setNumStopBits(int newStopBits) { stopBits = newStopBits; configPort(); }
	public void setParity(int newParity) { parity = newParity; configPort(); }
	
	// Getters
	public String getDescriptivePortName() { return portString.trim(); }
	public String getSystemPortName() { return comPort.substring(comPort.lastIndexOf('\\')+1); }
	public int getBaudRate() { return baudRate; }
	public int getByteSize() { return byteSize; }
	public int getNumStopBits() { return stopBits; }
	public int getParity() { return parity; }
	
	private void fixAndSetComPort(String comPortName)
	{
		comPort = "\\\\.\\";
		if (comPortName.charAt(0) != '\\')
			comPort += comPortName;
		else
			comPort = comPortName;
	}
	
	// InputStream interface class
	private class SerialCommInputStream extends InputStream
	{
		@Override
		public int available() throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			return 1;
		}
		
		@Override
		public int read() throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[1];
			return (readBytes(buffer, 1l) > 0) ? buffer[0] : -1;
		}
		
		@Override
		public int read(byte[] b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			return read(b, 0, b.length);
			
		}
		
		@Override
		public int read(byte[] b, int off, int len) throws IOException
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
		public long skip(long n) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[(int)n];
			return readBytes(buffer, n);
		}
	}
	
	// OutputStream interface class
	private class SerialCommOutputStream extends OutputStream
	{
		@Override
		public void write(int b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			byte[] buffer = new byte[1];
			buffer[0] = (byte)(b & 0x000000FF);
			if (writeBytes(buffer, 1l) < 0)
				throw new IOException("This port appears to have been shutdown or disconnected.");
		}
		
		@Override
		public void write(byte[] b) throws IOException
		{
			if (!isOpened)
				throw new IOException("This port appears to have been shutdown or disconnected.");
			
			write(b, 0, b.length);
		}
		
		@Override
		public void write(byte[] b, int off, int len) throws IOException
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
		ArrayList<SerialComm> ports = SerialComm.getCommPorts();
		SerialComm ubxPort = ports.get(1);
		
		ubxPort.openPort();
		InputStream in = ubxPort.getInputStream();try{
		for (long i = 0; i < 1000; ++i)
			System.out.print((char)in.read());}catch (Exception e) {}
		
		ubxPort.closePort();
	}
}
