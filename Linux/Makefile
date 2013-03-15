# Linux specific library variables
COMPILE			:= g++
LINK			:= g++
ALL_CFLAGS		:= -fPIC
ALL_LDFLAGS		:= -fPIC -shared
INCLUDES		:= -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
LIBRARIES		:= 
DELETE			:= @rm
MKDIR			:= @mkdir
PRINT			:= @echo
FULL_CLASS		:= j.extensions.comm.SerialComm
JAVAC			:= $(JAVA_HOME)/bin/javac
JAVAH			:= $(JAVA_HOME)/bin/javah -jni
JFLAGS 			:= -source 1.5 -target 1.5 -Xlint:-options
LIBRARY_NAME	:= libSerialComm.so
SOURCES			:= SerialComm_Linux.cpp
OBJECTSx86		:= $(patsubst %.cpp,x86/%.o,$(SOURCES))
OBJECTSx86_64	:= $(patsubst %.cpp,x86_64/%.o,$(SOURCES))
JNI_HEADER		:= ../j_extensions_comm_SerialComm.h
JAVA_CLASS		:= ../j/extensions/comm/SerialComm.class

# Define phony and suffix rules
.PHONY: all linux32 linux64 checkdirs clean clobber
.SUFFIXES:
.SUFFIXES: .cpp .o .class .java .h

# Default build target builds both 32 and 64 bit libraries
all : linux32 linux64

# Builds 32-bit Linux libraries
linux32 : ARCH = -m32
linux32 : checkdirs x86/$(LIBRARY_NAME)
	$(DELETE) -rf x86/*.o

# Builds 64-bit Linux libraries
linux64 : ARCH = -m64
linux64 : checkdirs x86_64/$(LIBRARY_NAME)
	$(DELETE) -rf x86_64/*.o

# Rule to create build directories
checkdirs : x86 x86_64
x86 :
	$(MKDIR) -p $@
x86_64 :
	$(MKDIR) -p $@

# Rule to build 32-bit library
x86/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86)
	$(CC) $(LDFLAGS) $(ALL_LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86)

# Rule to build 64-bit library
x86_64/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86_64)
	$(CC) $(LDFLAGS) $(ALL_LDFLAGS) $(ARCH) -o $@ $(OBJECTSx86_64)
	
# Suffix rules to get from *.cpp -> *.o
x86/%.o : %.cpp
	$(CC) $(INCLUDES) $(CFLAGS) $(ALL_CFLAGS) $(ARCH) -c $< -o $@
x86_64/%.o : %.cpp
	$(CC) $(INCLUDES) $(CFLAGS) $(ALL_CFLAGS) $(ARCH) -c $< -o $@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)
	$(JAVAH) -d .. -classpath .. $(FULL_CLASS)

# Suffix rule to get from *.java -> *.class
.java.class :
	$(JAVAC) $(JFLAGS) $(@:.class=.java)

# Rules to clean source directories
clean :
	$(DELETE) -rf ./*.o ../*.h

clobber : clean
	$(DELETE) -rf ./x86 ./x86_64
