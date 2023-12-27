TARGET = libdrakonsound.a
OBJS = Nogg/src/util/memory.o \
  Nogg/src/util/float-to-int16.o \
  Nogg/src/util/decode-frame.o \
  Nogg/src/decode/stb_vorbis.o \
  Nogg/src/decode/setup.o \
  Nogg/src/decode/seek.o \
  Nogg/src/decode/packet.o \
  Nogg/src/decode/io.o \
  Nogg/src/decode/decode.o \
  Nogg/src/decode/crc32.o \
  Nogg/src/api/version.o \
  Nogg/src/api/seek-tell.o \
  Nogg/src/api/read-int16.o \
  Nogg/src/api/read-float.o \
  Nogg/src/api/open-file.o \
  Nogg/src/api/open-callbacks.o \
  Nogg/src/api/open-buffer.o \
  Nogg/src/api/info.o \
  Nogg/src/api/close.o \
  DrakonSound/DrakonWavFile.o \
  DrakonSound/DrakonFileBuffer.o \
  DrakonSound/DrakonSound.o


PHONY := all package clean
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

PREFIX := arm-vita-eabi
CC := arm-vita-eabi-gcc
CXX := arm-vita-eabi-g++
STRIP := arm-vita-eabi-strip
AR := arm-vita-eabi-ar

PROJECT := drakonsound
CFLAGS = -Wl,-q -O2 -std=c99 -I./Nogg -I./include
CXXFLAGS = -Wl,-q -O2 -std=c++11 -I./Nogg -I./include


all: $(TARGET) install

$(TARGET): $(OBJS)
	$(AR) -rc $@ $^

install: $(TARGET)
	mv $(TARGET) $(VITASDK)/$(PREFIX)/lib/$(TARGET)
	@mkdir -p $(VITASDK)/$(PREFIX)/include/DrakonSound
	cp include/DrakonSound/*.h $(VITASDK)/$(PREFIX)/include/DrakonSound
	@mkdir -p $(VITASDK)/$(PREFIX)/include/clownresampler
	cp include/clownresampler/*.h $(VITASDK)/$(PREFIX)/include/clownresampler

%.o : %.cpp
	arm-vita-eabi-g++ -c $(CXXFLAGS) -o $@ $<

%.o : %.c
	arm-vita-eabi-gcc -c $(CFLAGS) -o $@ $<

clean:
	-rm -f $(PROJECT)
	-rm -r *.o
	-rm -r DrakonSound/*.o
	-rm -r Nogg/src/util/*.o
	-rm -r Nogg/src/decode/*.o
	-rm -r Nogg/src/api/*.o

