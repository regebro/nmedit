AM_CXXFLAGS = -DALSA -DNETMIDI -DLIBPATH='"$(pkglibdir)"'
INCLUDES = -I.. -I../../libpdl -I../../libnmpatch

CODECS_DIR = ../../codecs
EXTRA_DIST = $(CODECS_DIR)/patch.pdl $(CODECS_DIR)/midi.pdl

lib_LTLIBRARIES = libnmprotocol.la

libnmprotocol_la_SOURCES = netmididriver.cc alsadriver.cc iammessage.cc lightmessage.cc mididriver.cc midimessage.cc nmprotocol.cc patchmessage.cc ackmessage.cc requestpatchmessage.cc getpatchlistmessage.cc patchlistmessage.cc newpatchinslotmessage.cc voicecountmessage.cc getpatchmessage.cc slotsselectedmessage.cc slotactivatedmessage.cc activepidlistener.cc parametermessage.cc synth.cc

libnmprotocol_la_LIBADD = -L../../libpdl/src -lpdl
libnmprotocol_la_LDFLAGS = -release 1 -no-undefined -static

pdlfiledir = $(pkglibdir)
pdlfile_DATA = $(CODECS_DIR)/patch.pdl $(CODECS_DIR)/midi.pdl
