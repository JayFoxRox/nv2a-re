# Make sure that NXDK_DIR=<path> and DEBUG=<y/n> are set properly!

XBE_TITLE=nv2a-re

SRCS += $(wildcard $(CURDIR)/*.c)
#SHADER_OBJS = ps.inl vs.inl

NXDK_NET=y
include $(NXDK_DIR)/Makefile


# The following are remains of trying to compile C++

#CXXFLAGS = -target i386-pc-win32 -march=pentium3 \
#               -ffreestanding -nostdlib -fno-builtin -fno-exceptions \
#               -I$(NXDK_DIR)/lib -I$(NXDK_DIR)/lib/xboxrt \
#               -Wno-ignored-attributes \
#               -I/lib/clang/3.9.1/include/

#CXXFLAGS = -target i386-pc-win32 -march=pentium3 \
#               -fno-exceptions \
#               -fno-ms-compatibility \
#               -fno-short-wchar \
#               -I$(NXDK_DIR)/lib -I$(NXDK_DIR)/lib/xboxrt \
#               -I/home/fox/Data/Projects/nxdk/include/ \
#               -I/usr/i686-w64-mingw32/include/c++/6.3.1/ \
#               -I/usr/i686-w64-mingw32/include/c++/6.3.1/tr1/ \
#               -I/usr/include/ \
#               -I/usr/i686-w64-mingw32/include/ \
#               -I/usr/i686-w64-mingw32/include/c++/6.3.1/i686-w64-mingw32/
#               -I/usr/include/c++/6.3.1/x86_64-pc-linux-gnu/ \
               #-I/usr/i686-w64-mingw32/include/c++/6.3.1/i686-w64-mingw32/ \
               #-Wno-ignored-attributes \
               #-I/lib/clang/3.9.1/include/

#main.obj:main.cpp
#	@echo "[ CXX      ] $@"
##	$(VE) $(CXX) -std=c++11 $(CXXFLAGS) -c -o '$@' '$<'
#	$(VE) $(CXX) -std=c++11 $(CFLAGS) -c -o '$@' '$<'
