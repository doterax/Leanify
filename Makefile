LEANIFY_OBJ     := leanify.o main.o utils.o library.o fileio.o $(patsubst %.cpp,%.o,$(wildcard formats/*.cpp))
LZMA_OBJ        := lib/LZMA/Alloc.o lib/LZMA/CpuArch.o lib/LZMA/LzFind.o lib/LZMA/LzFindMt.o lib/LZMA/LzFindOpt.o lib/LZMA/LzmaDec.o lib/LZMA/LzmaEnc.o lib/LZMA/Threads.o
MOZJPEG_OBJ     := lib/mozjpeg/jaricom.o lib/mozjpeg/jcapimin.o lib/mozjpeg/jcarith.o lib/mozjpeg/jcext.o lib/mozjpeg/jchuff.o lib/mozjpeg/jcmarker.o lib/mozjpeg/jcmaster.o lib/mozjpeg/jcomapi.o lib/mozjpeg/jcparam.o lib/mozjpeg/jcphuff.o lib/mozjpeg/jctrans.o lib/mozjpeg/jdapimin.o lib/mozjpeg/jdarith.o lib/mozjpeg/jdatadst.o lib/mozjpeg/jdatasrc.o lib/mozjpeg/jdcoefct.o lib/mozjpeg/jdhuff.o lib/mozjpeg/jdinput.o lib/mozjpeg/jdmarker.o lib/mozjpeg/jdphuff.o lib/mozjpeg/jdtrans.o lib/mozjpeg/jerror.o lib/mozjpeg/jmemmgr.o lib/mozjpeg/jmemnobs.o lib/mozjpeg/jsimd_none.o lib/mozjpeg/jutils.o
PUGIXML_OBJ     := lib/pugixml/pugixml.o
ZOPFLI_OBJ      := lib/zopfli/hash.o lib/zopfli/squeeze.o lib/zopfli/gzip_container.o lib/zopfli/katajainen.o lib/zopfli/zopfli_lib.o lib/zopfli/cache.o lib/zopfli/zlib_container.o lib/zopfli/util.o lib/zopfli/tree.o lib/zopfli/deflate.o lib/zopfli/blocksplitter.o lib/zopfli/lz77.o
ZOPFLIPNG_OBJ   := lib/zopflipng/lodepng/lodepng.o lib/zopflipng/lodepng/lodepng_util.o lib/zopflipng/zopflipng_lib.o

CFLAGS      += -Wall -Werror -O3 -flto
CPPFLAGS    += -I./lib
CXXFLAGS    += $(CFLAGS) -std=c++17 -fno-rtti
LDFLAGS     += -flto

PREFIX ?= /usr/local
BINDIR := $(PREFIX)/bin
INSTALL ?= install

ifeq ($(OS), Windows_NT)
    SYSTEM  := Windows
    LDLIBS  += -lshell32
    CPPFLAGS += -D_CRT_SECURE_NO_WARNINGS
    # Clang on Windows requires lld for LTO
    ifneq ($(filter clang%,$(CC)),)
        LDFLAGS += -fuse-ld=lld
    else
        LDFLAGS += -s -static
    endif
else
    SYSTEM  := $(shell uname -s)
    LDFLAGS += -lpthread
endif

# -lstdc++fs supported only on Linux
ifeq ($(SYSTEM), Linux)
    LDFLAGS += -lstdc++fs
endif

ifeq ($(SYSTEM), Darwin)
    LDLIBS  += -liconv
else ifeq ($(SYSTEM), Linux)
    # -s is "obsolete" on mac, not supported by lld on Windows
    LDFLAGS += -s -static
endif

ifeq ($(SYSTEM), Windows)
    TARGET := leanify.exe
else
    TARGET := leanify
endif

.PHONY:     leanify asan install uninstall clean

leanify:    $(LEANIFY_OBJ) $(LZMA_OBJ) $(MOZJPEG_OBJ) $(PUGIXML_OBJ) $(ZOPFLI_OBJ) $(ZOPFLIPNG_OBJ)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $(TARGET)

$(LEANIFY_OBJ): CFLAGS += -Wextra -Wno-unused-parameter -Wno-c++20-extensions

$(LZMA_OBJ):    CFLAGS += -Wno-unknown-warning-option -Wno-dangling-pointer

$(ZOPFLI_OBJ):  CFLAGS += -Wno-unused-function

asan: CFLAGS += -g -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS := -fsanitize=address $(filter-out -s -static,$(LDFLAGS))
asan: leanify

debug: CFLAGS := $(filter-out -O3,$(CFLAGS)) -O0 -g -fno-omit-frame-pointer
debug: LDFLAGS := $(filter-out -s -flto -static,$(LDFLAGS))
debug: CFLAGS := $(filter-out -flto,$(CFLAGS))
debug: leanify

install: leanify
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/leanify

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/leanify

clean:
	rm -f $(LEANIFY_OBJ) $(LZMA_OBJ) $(MOZJPEG_OBJ) $(PUGIXML_OBJ) $(ZOPFLI_OBJ) $(ZOPFLIPNG_OBJ) $(TARGET)
