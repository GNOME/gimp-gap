## Process this file with automake to produce Makefile.in

libexecdir = $(GIMP_PLUGIN_DIR)/plug-ins

presetdatadir = $(GIMP_DATA_DIR)/video_encoder_presets
presetdata_DATA =  \
        ff_divx_hq.preset \
        ff_divx_low.preset \
        ff_divx_standard.preset \
        ff_divx_win.preset \
        ff_mpeg1_hq.preset \
        ff_mpeg1_vcd.preset \
        ff_mpeg2_dvd.preset \
        ff_mpeg2_svcd.preset \
        ff_real_standard.preset \
        ff_x264_aac_hq.preset \
        ff_x264_hq.preset



# we depend on libavformat / ibavcodec to build that encoder
# (without those libs the ffmpeg encoder can not be built at all)
if ENABLE_GVA_LIBAVFORMAT_SUPPORT

if GAP_VIDEOAPI_SUPPORT
GAPVIDEOAPI = -L$(top_builddir)/libgapvidapi -lgapvidapi  $(GAPVIDEOAPI_EXTLIBS)
INC_GAPVIDEOAPI = -I$(top_srcdir)/libgapvidapi $(GAPVIDEOAPI_EXTINCS)
endif

LIBGAPBASE  = $(top_builddir)/libgapbase/libgapbase.a
INC_LIBGAPBASE = -I$(top_srcdir)/libgapbase

LIBGAPSTORY = -L$(top_builddir)/gap -lgapstory
INC_LIBGAPSTORY = -I$(top_srcdir)/gap

LIBGAPVIDUTIL  = -L$(top_builddir)/libgapvidutil -lgapvidutil
INC_LIBGAPVIDUTIL = -I$(top_srcdir)/libgapvidutil


libexec_PROGRAMS = gap_vid_enc_ffmpeg

gap_vid_enc_ffmpeg_SOURCES = \
	gap_enc_ffmpeg_main.c		\
	gap_enc_ffmpeg_main.h		\
	gap_enc_ffmpeg_gui.c		\
	gap_enc_ffmpeg_gui.h		\
	gap_enc_ffmpeg_par.h		\
	gap_enc_ffmpeg_par.c		\
	gap_enc_ffmpeg_callbacks.c	\
	gap_enc_ffmpeg_callbacks.h	


AM_CPPFLAGS = \
	-DGAPLIBDIR=\""$(GAPLIBDIR)"\"	\
	-DLOCALEDIR=\""$(LOCALEDIR)"\"

INCLUDES = \
	-I$(top_srcdir)	\
	-I$(top_srcdir)/libwavplayclient	\
	$(INC_LIBGAPBASE)	\
	$(INC_GAPVIDEOAPI)	\
	$(INC_LIBGAPSTORY)	\
	$(INC_LIBGAPVIDUTIL)	\
	$(GIMP_CFLAGS)		\
	-I$(includedir)


LDADD = $(GIMP_LIBS)


# note: sequence of libs matters because LIBGAPVIDUTIL uses both LIBGAPSTORY and GAPVIDEOAPI
#       (if those libs appear before LIBGAPVIDUTIL the linker can not resolve those references.

gap_vid_enc_ffmpeg_LDADD = $(LIBGAPVIDUTIL) $(LIBGAPSTORY) $(GAPVIDEOAPI) $(LIBGAPBASE) $(GIMP_LIBS)


EXTRA_DIST = $(presetdata_DATA)

endif
