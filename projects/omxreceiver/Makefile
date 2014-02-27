ROOTFS=$(HOME)/raspberrypi/rootfs
COMMON=../../common

OBJS=Main.o PictureAnalyser.o OsdGraphics.o OsdImage.o OmxVideoDecode.o vga.o $(COMMON)/StreamReader.o $(COMMON)/TsAnalyser.o $(COMMON)/PesAnalyser.o $(COMMON)/PesPacket.o
BIN=Main.bin
INCLUDES+=-I$(COMMON) -I$(ROOTFS)/opt/vc/src/hello_pi/libs/ilclient -I$(ROOTFS)/opt/vc/src/hello_pi/libs/vgfont
LDFLAGS+=-lstdc++ -lilclient -L$(ROOTFS)/opt/vc/src/hello_pi/libs/ilclient -L$(ROOTFS)/opt/vc/src/hello_pi/libs/vgfont
# LDFLAGS+=-lvgfont -lfreetype -lz

include $(ROOTFS)/opt/vc/src/hello_pi/Makefile.include
