package j.extensions.comm;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * This class provides native access to serial ports and devices without requiring external libraries or tools.
 * 
 * @author Will Hedgecock <will.hedgecock@gmail.com>
 * @version 1.0
 * @see java.io.InputStream
 * @see java.io.OutputStream
 */
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
				copyFromPath = "Windows/x86_64";
			else
				copyFromPath = "Windows/x86";
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
				copyFromPath = "OSX/x86_64";
			else
				copyFromPath = "OSX/x86";
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
				copyFromPath = "Linux/x86_64";
			else
				copyFromPath = "Linux/x86";
			fileName = "libSerialComm.so";
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
		tempNativeLibrary = new File ("libSerialComm.so");
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
	
	/**
	 * Returns a list of all available serial ports on this machine.
	 * <p>
	 * The serial ports can be accessed by iterating through each of the SerialComm objects in this array.
	 * <p>
	 * Note that the {@link #openPort()} method must be called before any attempts to read from or write to the port.  Likewise, {@link #closePort()} should be called when you are finished accessing the port.
	 * <p>
	 * All serial port parameters or timeouts can be changed at any time after the port has been opened.
	 * 
	 * @return An array of SerialComm objects.
	 */
	static public native SerialComm[] getCommPorts();
	
	// Parity Values
	static final public int NO_PARITY = 0;
	static final public int ODD_PARITY = 1;
	static final public int EVEN_PARITY = 2;
	static final public int MARK_PARITY = 3;
	static final public int SPACE_PARITY = 4;

	// Number of Stop Bits
	static final public int ONE_STOP_BIT = 1;
	static final public int ONE_POINT_FIVE_STOP_BITS = 2;
	static final public int TWO_STOP_BITS = 3;
	
	// Flow Control constants
	static final public int FLOW_CONTROL_CTS_ENABLED = 0x00000001;
	static final public int FLOW_CONTROL_DSR_ENABLED = 0x00000010;
	static final public int FLOW_CONTROL_DTR_ENABLED = 0x00000100;
	static final public int FLOW_CONTROL_RTS_ENABLED = 0x00001000;
	static final public int FLOW_CONTROL_XONXOFF_ENABLED = 0x00010000;
	
	// Serial Port Parameters
	private volatile int baudRate = 9600, byteSize = 8, stopBits = ONE_STOP_BIT, parity = NO_PARITY;
	private volatile int readTimeout = 0, writeTimeout = 0, flowControl = 0;
	private volatile SerialCommInputStream inputStream = null;
	private volatile SerialCommOutputStream outputStream = null;
	private volatile String portString, comPort;
	private volatile long portHandle = -1l;
	private volatile boolean isOpened = false;
	
	/**
	 * Opens this serial port for reading and writing.
	 * <p>
	 * All serial port parameters or timeouts can be changed at any time after the port has been opened.
	 */
	public final native boolean openPort();
	
	/**
	 * Closes this serial port.
	 */
	public final native boolean closePort();
	
	// Serial Port Setup Methods
	private final native boolean configPort();							// Changes/sets serial port parameters as defined by this class
	private final native boolean configFlowControl();					// Changes/sets flow control parameters as defined by this class
	private final native boolean configTimeouts();						// Changes/sets serial port timeouts as defined by this class
	
	/**
	 * Reads up to <i>bytesToRead</i> raw data bytes from the serial port and stores them in the buffer.
	 * <p>
	 * The length of the byte buffer must be greater than or equal to the value passed in for <i>bytesToRead</i>
	 * <p>
	 * If no timeouts were specified or the read timeout was set to 0, this call will block until <i>bytesToRead</i> bytes of data have been successfully read from the serial port.
	 * Otherwise, this method will return after <i>bytesToRead</i> bytes of data have been read or the number of milliseconds specified by the read timeout have elapsed, 
	 * whichever comes first, regardless of the availability of more data.
	 * 
	 * @param buffer The buffer into which the raw data is read.
	 * @param bytesToRead The number of bytes to read from the serial port.
	 * @return The number of bytes successfully read, or -1 if there was an error reading from the port.
	 */
	public final native int readBytes(byte[] buffer, long bytesToRead);
	
	/**
	 * Writes up to <i>bytesToWrite</i> raw data bytes from the buffer parameter to the serial port.
	 * <p>
	 * The length of the byte buffer must be greater than or equal to the value passed in for <i>bytesToWrite</i>
	 * <p>
	 * If no timeouts were specified or the write timeout was set to 0, this call will block until <i>bytesToWrite</i> bytes of data have been successfully written the serial port.
	 * Otherwise, this method will return after <i>bytesToWrite</i> bytes of data have been written or the number of milliseconds specified by the write timeout have elapsed, 
	 * whichever comes first, regardless of the availability of more data.
	 * 
	 * @param buffer The buffer containing the raw data to write to the serial port.
	 * @param bytesToWrite The number of bytes to write to the serial port.
	 * @return The number of bytes successfully written, or -1 if there was an error writing to the port.
	 */
	public final native int writeBytes(byte[] buffer, long bytesToWrite);
	
	// Default Constructor
	public SerialComm() {}
	
	/**
	 * Returns an {@link java.io.InputStream} object associated with this serial port.
	 * <p>
	 * Allows for easier read access of the underlying data stream and abstracts away many low-level read details.
	 * <p>
	 * Make sure to call the {@link java.io.InputStream#close()} method when you are done using this stream.
	 * 
	 * @return An {@link java.io.InputStream} object associated with this serial port.
	 * @see java.io.InputStream
	 */
	public final InputStream getInputStream()
	{
		if ((inputStream == null) && isOpened)
			inputStream = new SerialCommInputStream();
		return inputStream;
	}
	
	/**
	 * Returns an {@link java.io.OutputStream} object associated with this serial port.
	 * <p>
	 * Allows for easier write access to the underlying data stream and abstracts away many low-level writing details.
	 * <p>
	 * Make sure to call the {@link java.io.OutputStream#close()} method when you are done using this stream.
	 * 
	 * @return An {@link java.io.OutputStream} object associated with this serial port.
	 * @see java.io.OutputStream
	 */
	public final OutputStream getOutputStream()
	{
		if ((outputStream == null) && isOpened)
			outputStream = new SerialCommOutputStream();
		return outputStream;
	}
	
	/**
	 * Sets all serial port parameters at one time.
	 * <p>
	 * Allows the user to set all port parameters with a single function call.
	 * <p>
	 * The baud rate can be any arbitrary value specified by the user.  The default value is 9600 baud.  The byte size
	 * specifies how many data bits to use per word.  The default is 8, but any values from 5 to 8 are acceptable.
	 * <p>
	 * The default number of stop bits is 1, but 2 bits can also be used or even 1.5 on Windows machines.  Please use the built-in
	 * constants for this parameter ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 * <p>
	 * The parity parameter specifies how error detection is carried out.  Again, the built-in constants should be used.
	 * Acceptable values are {@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}.
	 * 
	 * @param newBaudRate The desired baud rate for this serial port.
	 * @param newByteSize The number of data bits to use per word.
	 * @param newStopBits The number of stop bits to use.
	 * @param newParity The type of parity error-checking desired.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final void setComPortParameters(int newBaudRate, int newByteSize, int newStopBits, int newParity)
	{
		baudRate = newBaudRate;
		byteSize = newByteSize;
		stopBits = newStopBits;
		parity = newParity;
		configPort();
	}
	
	/**
	 * Sets the serial port read and write timeout parameters.
	 * <p>
	 * A value of 0 indicates that a {@link #readBytes(byte[],long)} or {@link #writeBytes(byte[],long)} call
	 * should block forever until it has successfully read or written the indicated number of bytes to the serial port.
	 * <p>
	 * Any value other than 0 indicates that a {@link #readBytes(byte[],long)} or {@link #writeBytes(byte[],long)} call
	 * will return after <i>newReadTimeout</i> or <i>newWriteTimeout</i> milliseconds of inactivity, regardless of the
	 * availability of more data or the number of bytes successfully written or read.
	 * 
	 * @param newReadTimeout The number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 * @param newWriteTimeout The number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 */
	public final void setComPortTimeouts(int newReadTimeout, int newWriteTimeout)
	{
		readTimeout = newReadTimeout;
		writeTimeout = newWriteTimeout;
		configTimeouts();
	}
	
	/**
	 * Sets the desired baud rate for this serial port.
	 * <p>
	 * The default baud rate is 9600 baud.
	 * 
	 * @param newBaudRate The desired baud rate for this serial port.
	 */
	public final void setBaudRate(int newBaudRate) { baudRate = newBaudRate; configPort(); }
	
	/**
	 * Sets the desired number of data bits per word.
	 * <p>
	 * The default number of data bits per word is 8.
	 * 
	 * @param newByteSize The desired number of data bits per word.
	 */
	public final void setByteSize(int newByteSize) { byteSize = newByteSize; configPort(); }
	
	/**
	 * Sets the desired number of stop bits per word.
	 * <p>
	 * The default number of stop bits per word is 2.  Built-in stop-bit constants should be used
	 * in this method ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 * <p>
	 * Note that {@link #ONE_POINT_FIVE_STOP_BITS} stop bits may not be available on non-Windows systems.
	 * 
	 * @param newStopBits The desired number of stop bits per word.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 */
	public final void setNumStopBits(int newStopBits) { stopBits = newStopBits; configPort(); }
	
	/**
	 * Specifies what kind of flow control to enable for this serial port.
	 * <p>
	 * By default, no flow control is enabled.  Built-in flow control constants should be used
	 * in this method ({@link #FLOW_CONTROL_RTS_ENABLED}, {@link #FLOW_CONTROL_CTS_ENABLED}, {@link #FLOW_CONTROL_DTR_ENABLED},
	 * {@link #FLOW_CONTROL_DSR_ENABLED}, {@link #FLOW_CONTROL_XONXOFF_ENABLED}), and can be OR'ed together.
	 * <p>
	 * Valid flow control configurations are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;CTS: {@link #FLOW_CONTROL_CTS_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;RTS/CTS: {@link #FLOW_CONTROL_RTS_ENABLED} | {@link #FLOW_CONTROL_CTS_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DSR: {@link #FLOW_CONTROL_DSR_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DTR/DSR: {@link #FLOW_CONTROL_DTR_ENABLED} | {@link #FLOW_CONTROL_DSR_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;XOn/XOff: {@link #FLOW_CONTROL_XONXOFF_ENABLED}
	 * <p>
	 * Note that only one valid flow control configuration can be used at any time.  For example, attempting to use both XOn/XOff
	 * <b>and</b> RTS/CTS will most likely result in an unusable serial port.
	 * 
	 * @param newFlowControlSettings The desired type of flow control to enable for this serial port.
	 * @see #FLOW_CONTROL_RTS_ENABLED
	 * @see #FLOW_CONTROL_CTS_ENABLED
	 * @see #FLOW_CONTROL_DTR_ENABLED
	 * @see #FLOW_CONTROL_DSR_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_ENABLED
	 */
	public final void setFlowControl(int newFlowControlSettings) { flowControl = newFlowControlSettings; configFlowControl(); }
	
	/**
	 * Sets the desired parity error-detection scheme to be used.
	 * <p>
	 * The parity parameter specifies how error detection is carried out.  The built-in parity constants should be used.
	 * Acceptable values are {@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}.
	 * 
	 * @param newParity The desired parity scheme to be used.
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final void setParity(int newParity) { parity = newParity; configPort(); }
	
	/**
	 * Gets a descriptive string representing this serial port or the device connected to it.
	 * <p>
	 * This description is generated by the operating system and may or may not be a good representation of the actual port or
	 * device it describes.
	 * 
	 * @return A descriptive string representing this serial port.
	 */
	public final String getDescriptivePortName() { return portString.trim(); }
	
	/**
	 * Gets the operating system-defined device name corresponding to this serial port.
	 * 
	 * @return The system-defined device name of this serial port.
	 */
	public final String getSystemPortName() { return comPort.substring(comPort.lastIndexOf('\\')+1); }
	
	/**
	 * Gets the current baud rate of the serial port.
	 * 
	 * @return The current baud rate of the serial port.
	 */
	public final int getBaudRate() { return baudRate; }
	
	/**
	 * Gets the current number of data bits per word.
	 * 
	 * @return The current number of data bits per word.
	 */
	public final int getByteSize() { return byteSize; }
	
	/**
	 * Gets the current number of stop bits per word.
	 * <p>
	 * The return value should not be interpreted as an integer, but rather compared with the built-in stop bit constants
	 * ({@link #ONE_STOP_BIT}, {@link #ONE_POINT_FIVE_STOP_BITS}, {@link #TWO_STOP_BITS}).
	 * 
	 * @return The current number of stop bits per word.
	 * @see #ONE_STOP_BIT
	 * @see #ONE_POINT_FIVE_STOP_BITS
	 * @see #TWO_STOP_BITS
	 */
	public final int getNumStopBits() { return stopBits; }
	
	/**
	 * Gets the current parity error-checking scheme.
	 * <p>
	 * The return value should not be interpreted as an integer, but rather compared with the built-in parity constants
	 * ({@link #NO_PARITY}, {@link #EVEN_PARITY}, {@link #ODD_PARITY}, {@link #MARK_PARITY}, and {@link #SPACE_PARITY}).
	 * 
	 * @return The current parity scheme.
	 * @see #NO_PARITY
	 * @see #EVEN_PARITY
	 * @see #ODD_PARITY
	 * @see #MARK_PARITY
	 * @see #SPACE_PARITY
	 */
	public final int getParity() { return parity; }
	
	/**
	 * Gets the number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 * <p>
	 * A value of 0 indicates that a {@link #readBytes(byte[],long)} call will block forever until it has successfully read
	 * the indicated number of bytes from the serial port.
	 * <p>
	 * Any value other than 0 indicates the number of milliseconds of inactivity that will be tolerated before the {@link #readBytes(byte[],long)}
	 * call will return.
	 * 
	 * @return The number of milliseconds of inactivity to tolerate before returning from a {@link #readBytes(byte[],long)} call.
	 */
	public final int getReadTimeout() { return readTimeout; }
	
	/**
	 * Gets the number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 * <p>
	 * A value of 0 indicates that a {@link #writeBytes(byte[],long)} call will block forever until it has successfully written
	 * the indicated number of bytes to the serial port.
	 * <p>
	 * Any value other than 0 indicates the number of milliseconds of inactivity that will be tolerated before the {@link #writeBytes(byte[],long)}
	 * call will return.
	 * 
	 * @return The number of milliseconds of inactivity to tolerate before returning from a {@link #writeBytes(byte[],long)} call.
	 */
	public final int getWriteTimeout() { return writeTimeout; }
	
	/**
	 * Returns the flow control settings enabled on this serial port.
	 * <p>
	 * The integer result should be masked with the built-in flow control constants to test if the desired setting is enabled.
	 * Valid flow control configurations are:
	 * <p>
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;CTS: {@link #FLOW_CONTROL_CTS_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;RTS/CTS: {@link #FLOW_CONTROL_RTS_ENABLED} | {@link #FLOW_CONTROL_CTS_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DSR: {@link #FLOW_CONTROL_DSR_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;DTR/DSR: {@link #FLOW_CONTROL_DTR_ENABLED} | {@link #FLOW_CONTROL_DSR_ENABLED}<br />
	 * &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;XOn/XOff: {@link #FLOW_CONTROL_XONXOFF_ENABLED}
	 * 
	 * @return The flow control settings enabled on this serial port.
	 * @see #FLOW_CONTROL_RTS_ENABLED
	 * @see #FLOW_CONTROL_CTS_ENABLED
	 * @see #FLOW_CONTROL_DTR_ENABLED
	 * @see #FLOW_CONTROL_DSR_ENABLED
	 * @see #FLOW_CONTROL_XONXOFF_ENABLED
	 */
	public final int getFlowControlSettings() { return flowControl; }
	
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
			return (readBytes(buffer, 1l) > 0) ? ((int)buffer[0] & 0x000000FF) : -1;
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
		SerialComm ubxPort = ports[2];
		
		byte[] readBuffer = new byte[2048];
		System.out.println("Opening " + ubxPort.getDescriptivePortName() + ": " + ubxPort.openPort());
		//ubxPort.setComPortTimeouts(500, 0);
		InputStream in = ubxPort.getInputStream();
		try
		{
			//for (int i = 0; i < 3; ++i)
			//{
				//System.out.println("\nReading #" + i);
				//int numRead = ubxPort.readBytes(readBuffer, readBuffer.length);
				//System.out.println("Read " + numRead + " bytes.");
				
				for (int j = 0; j < 1000; ++j)
					System.out.print((char)in.read());
			//}
			in.close();
		} catch (Exception e) { e.printStackTrace(); }
		
		System.out.println("\n\nClosing " + ubxPort.getDescriptivePortName() + ": " + ubxPort.closePort());
		System.out.println("Reopening " + ubxPort.getDescriptivePortName() + ": " + ubxPort.openPort() + "\n");
		in = ubxPort.getInputStream();
		try
		{
			for (int j = 0; j < 1000; ++j)
				System.out.print((char)in.read());
			in.close();
		} catch (Exception e) { e.printStackTrace(); }
		
		System.out.println("\nClosing " + ubxPort.getDescriptivePortName() + ": " + ubxPort.closePort());
	}
}
