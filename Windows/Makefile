# Windows specific library variables
COMPILE			:= cl
LINK			:= link
ALL_CFLAGS		:= /c /O2 /GF /GL /MT /EHsc /fp:precise /J /nologo /TP
ALL_LDFLAGS		:= /DLL /LTCG /NOASSEMBLY /NOLOGO
INCLUDES		:= /I"$(JAVA_HOME)/include" /I"$(JAVA_HOME)/include/win32"
LIBRARIES		:= Advapi32.lib
DELETE			:= @cmd /C del /Q /F
MKDIR			:= @mkdir
PRINT			:= @echo
FULL_CLASS		:= j.extensions.comm.SerialComm
JAVAC			:= "$(JAVA_HOME)/bin/javac"
JAVAH			:= "$(JAVA_HOME)/bin/javah" -jni
JFLAGS 			:= -source 1.5 -target 1.5 -Xlint:-options
LIBRARY_NAME	:= SerialComm.dll
SOURCES			:= SerialComm_Windows.cpp
OBJECTSx86		:= $(patsubst %.cpp,x86/%.o,$(SOURCES))
OBJECTSx86_64	:= $(patsubst %.cpp,x86_64/%.o,$(SOURCES))
JNI_HEADER		:= ../j_extensions_comm_SerialComm.h
JAVA_CLASS		:= ../j/extensions/comm/SerialComm.class

# Define phony and suffix rules
.PHONY: all win32 win64 checkdirs clean clobber
.SUFFIXES:
.SUFFIXES: .cpp .o .class .java .h

# Default build target builds both 32 and 64 bit libraries
all :
	$(PRINT) ""
	$(PRINT) "Must specify a target (either win32 or win64), but not both at the same time since different versions of the Microsoft Compiler are required for difference architectures."

# Builds 32-bit Windows libraries
win32 : checkdirs x86/$(LIBRARY_NAME)
	$(DELETE) x86\\*.o

# Builds 64-bit Windows libraries
win64 : checkdirs x86_64/$(LIBRARY_NAME)
	$(DELETE) x86_64\\*.o

# Rule to create build directories
checkdirs : x86 x86_64
x86 :
	$(MKDIR) -p $@
x86_64 :
	$(MKDIR) -p $@

# Rule to build 32-bit library
x86/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86)
	$(LINK) $(LDFLAGS) $(ALL_LDFLAGS) /MACHINE:X86 /OUT:$@ $(OBJECTSx86) $(LIBRARIES)

# Rule to build 64-bit library
x86_64/$(LIBRARY_NAME) : $(JNI_HEADER) $(OBJECTSx86_64)
	$(LINK) $(LDFLAGS) $(ALL_LDFLAGS) /MACHINE:X64 /OUT:$@ $(OBJECTSx86_64) $(LIBRARIES)
	
# Suffix rules to get from *.cpp -> *.o
x86/%.o : %.cpp
	$(COMPILE) $(CFLAGS) $(ALL_CFLAGS) $(INCLUDES) $< -Fo$@
x86_64/%.o : %.cpp
	$(COMPILE) $(CFLAGS) $(ALL_CFLAGS) $(INCLUDES) $< -Fo$@

# Rule to build JNI header file
$(JNI_HEADER) : $(JAVA_CLASS)
	$(JAVAH) -d .. -classpath .. $(FULL_CLASS)

# Suffix rule to get from *.java -> *.class
.java.class :
	$(JAVAC) $(JFLAGS) $(@:.class=.java)

# Rules to clean source directories
clean :
	$(DELETE) x86\\*.o
	$(DELETE) x86_64\\*.o
	$(DELETE) ..\\*.h

clobber : clean
	$(DELETE) x86
	$(DELETE) x86_64
