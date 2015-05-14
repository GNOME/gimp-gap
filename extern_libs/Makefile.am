## Process this file with automake to produce Makefile.in
#
# processing with automake
# will result in a makefile that calls the makefiles for the
# external libraries. 
# (the sourcecode of those libs is included in the gimp-gap distribution
#  to provide compiling and static linking of the required versions that are
#  tested to work with gimp-gap.)
#
# unfortnately those libs are not autotools compatible
# but they should compile on different operating systems with their original makefiles. 
# (at least ffmpeg)
# this workaround integrates those libs into the gimp-gap make concept
# without any changes in the libs.
#
# it creates the libraries via faking scriptdata.
# the libs are installed to GAPLIBDIR ...
# .. but immediate removed in the install-data-hook 
#    (due to static linking there is no need not to install them)

scriptdatadir = $(GAPLIBDIR)

#if ENABLE_GVA_LIBAVFORMAT_SUPPORT
if BUILD_FFMPEG_LIBS
x_ffmpeg_libavformat = $(FFMPEG_LIBAVFORMAT_A)
x_ffmpeg_libavcodec  = $(FFMPEG_LIBAVCODEC_A)
x_ffmpeg_libavutil  = $(FFMPEG_LIBAVUTIL_A)
endif

#if ENABLE_GVA_LIBMPEG3_SUPPORT
if BUILD_LIBMPEG3_LIB
x_libmpeg3 = $(LMPEG3_A)
endif

scriptdata_DATA = \
	$(x_ffmpeg_libavformat)			\
	$(x_ffmpeg_libavcodec)			\
	$(x_ffmpeg_libavutil)			\
	$(x_libmpeg3)


#if ENABLE_GVA_LIBAVFORMAT_SUPPORT
if BUILD_FFMPEG_LIBS
$(x_ffmpeg_libavformat):
	echo "cd $(FFMPEG_DIR)"; \
	cd $(FFMPEG_DIR);\
	$(EXGMAKE)

$(x_ffmpeg_libavcodec):
	echo "cd $(FFMPEG_DIR)"; \
	cd $(FFMPEG_DIR);\
	$(EXGMAKE)

$(x_ffmpeg_libavutil):
	echo "cd $(FFMPEG_DIR)"; \
	cd $(FFMPEG_DIR);\
	$(EXGMAKE)

endif

#if ENABLE_GVA_LIBMPEG3_SUPPORT
if BUILD_LIBMPEG3_LIB
$(x_libmpeg3):
	echo "cd $(LMPEG3_DIR)"; \
	cd $(LMPEG3_DIR); \
	$(EXGMAKE)

endif

install-data-hook:
	rm -f $(DESTDIR)$(GAPLIBDIR)/libavformat.a \
	$(DESTDIR)$(GAPLIBDIR)/libavcodec.a \
	$(DESTDIR)$(GAPLIBDIR)/libavutil.a \
	$(DESTDIR)$(GAPLIBDIR)/libmpeg3.a

EXTRA_DIST = \
	README_extern_libs		\
	configure_options_ffmpeg.txt	\
	configure_options_libmpeg3.txt	\
	ffmpeg.tar.gz			\
	libmpeg3.tar.gz
	
	
# eof (without this line Makefile generation produces syntax error)


