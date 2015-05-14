## Process this file with automake to produce Makefile.in

if GAP_AUDIO_SUPPORT_WAVPLAY
LIBWAVCLIENT=libwavplayclient
endif

if GAP_VIDEOAPI_SUPPORT
LIBGAPVIDAPI=libgapvidapi
endif

# note: videoencoders all use GAP_VIDEOAPI_SUPPORT
#       for storyboard based encoding, but this is optional
#       The storyboard processor can be configured without GAP_VIDEOAPI_SUPPORT
#       with the restiriction that cliptype MOVIE is not supported in that case.
#       therefore compilation of the video encoders no longer depends on GAP_VIDEOAPI_SUPPORT
### if GAP_VIDEOAPI_SUPPORT
LIBGAPVIDUTIL=libgapvidutil
VID_COMMON=vid_common
VID_ENC_RAWFRAMES=vid_enc_rawframes
VID_ENC_SINGLE=vid_enc_single
VID_ENC_AVI=vid_enc_avi
##  endif

# the ffmpeg based video encoder depends on ENABLE_GVA_LIBAVFORMAT_SUPPORT
# and is not bulit when disabled.
if ENABLE_GVA_LIBAVFORMAT_SUPPORT
VID_ENC_FFMPEG=vid_enc_ffmpeg
endif


SUBDIRS = libgapbase extern_libs images $(LIBWAVCLIENT) $(LIBGAPVIDAPI) gap po docs \
	$(LIBGAPVIDUTIL) 	\
	$(VID_COMMON)		\
	$(VID_ENC_AVI) 		\
	$(VID_ENC_FFMPEG) 	\
	$(VID_ENC_RAWFRAMES) 	\
	$(VID_ENC_SINGLE)

EXTRA_DIST = \
	AUTHORS			\
	COPYING			\
	INSTALL			\
	README			\
	ChangeLog		\
	ChangeLog.pre-1-2	\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

