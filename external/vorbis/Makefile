# Makefile of the library "external/vorbis"
# David Weenink, 21 December 2020

include ../../makefile.defs

CPPFLAGS = -I ../../melder

OBJECTS = ogg_bitwise.o ogg_framing.o \
	vorbis_block.o vorbis_bitrate.o vorbis_codebook.o vorbis_envelope.o vorbis_floor0.o \
	vorbis_info.o vorbis_mdct.o vorbisfile.o vorbis_floor0.o vorbis_floor1.o vorbis_lpc.o vorbis_lsp.o \
	vorbis_mapping0.o vorbis_psy.o vorbis_registry.o vorbis_res0.o vorbis_sharedbook.o vorbis_smallft.o \
	vorbis_synthesis.o vorbis_window.o


.PHONY: all clean

all: libvorbis.a

clean:
	$(RM) $(OBJECTS)
	$(RM) libvorbis.a

libvorbis.a: $(OBJECTS)
	touch libvorbis.a
	rm libvorbis.a
	$(AR) cq libvorbis.a $(OBJECTS)
	$(RANLIB) libvorbis.a

$(OBJECTS): *.h ../../melder/*.h
