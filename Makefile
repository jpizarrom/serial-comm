# Java packaging variables
JAR			= jar
JARFLAGS	= cmf
JARNAME		= SerialComm.jar
PACKAGE		= j/extensions/comm
LIBS		= Linux/x86/* Linux/x86_64/* Linux/rpi/* Linux/bbb/* OSX/x86/* OSX/x86_64/* Windows/x86/* Windows/x86_64/*

# Define phony and suffix rules
.PHONY: all
.SUFFIXES:
.SUFFIXES: .jar

all :
	$(JAR) $(JARFLAGS) MANIFEST.MF $(JARNAME) $(PACKAGE)/*.class $(LIBS)
