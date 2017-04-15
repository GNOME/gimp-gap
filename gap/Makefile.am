## Process this file with automake to produce Makefile.in

libexecdir = $(GIMP_PLUGIN_DIR)/plug-ins

scriptdatadir = $(GIMP_DATA_DIR)/scripts

scriptdata_DATA = sel-to-anim-img.scm gap-dup-continue.scm

if GAP_UNIX_FRONTENDS
GAP_FRONTENDS = gap_frontends
endif

GAP_DECODE_MPLAYER_FRONTEND = gap_decode_mplayer

if GAP_AUDIO_SUPPORT_WAVPLAY
GAP_AUDIO_LIBS = $(top_builddir)/libwavplayclient/libwavplayclient.a
endif

if GAP_AUDIO_SUPPORT_SDL
GAP_SDL_AUDIO_EXTRASRC = gap_sdl_audioplayer.h gap_sdl_audioplayer.c
GAP_AUDIO_LIBS = $(GAP_AUDIO_SDL_LIBS)
endif


if GAP_VIDEOAPI_SUPPORT
GAPVIDEOAPI = $(top_builddir)/libgapvidapi/libgapvidapi.a  $(GAPVIDEOAPI_EXTLIBS)
INC_GAPVIDEOAPI = -I$(top_srcdir)/libgapvidapi $(GAPVIDEOAPI_EXTINCS)
GAP_VIDEO_EXTRACT = gap_video_extract
GAP_VIDEO_INDEX = gap_video_index
endif

LIBGAPBASE  = $(top_builddir)/libgapbase/libgapbase.a $(GTHREAD_LIBS)
INC_LIBGAPBASE = -I$(top_srcdir)/libgapbase

LIBGIMPGAP = libgimpgap.a

LIBGAPSTORY = libgapstory.a

noinst_LIBRARIES = $(LIBGIMPGAP) $(LIBGAPSTORY)

BASE_SOURCES = \
	gap-intl.h		\
	gap_accel_char.c	\
	gap_accel_char.h	\
	gap_accel_da.c		\
	gap_accel_da.h		\
	gap_arr_dialog.c	\
	gap_arr_dialog.h	\
	gap_audio_util.c	\
	gap_audio_util.h	\
	gap_audio_wav.c		\
	gap_audio_wav.h		\
	gap_colordiff.c	        \
	gap_colordiff.h 	\
	gap_colormask_exec.c	\
	gap_colormask_exec.h 	\
	gap_colormask_file.c	\
	gap_colormask_file.h 	\
        gap_edge_detection.c    \
        gap_edge_detection.h    \
	gap_geo.c		\
	gap_geo.h		\
	gap_image.c		\
	gap_image.h		\
	gap_layer_copy.c	\
	gap_layer_copy.h 	\
	gap_lib.c		\
	gap_lib.h		\
	gap_lib_common_defs.h	\
	gap_detail_tracking_exec.c	\
	gap_detail_tracking_exec.h 	\
	gap_detail_align_exec.c	\
	gap_detail_align_exec.h	\
	gap_locate.c		\
	gap_locate.h		\
	gap_locate2.c		\
	gap_locate2.h		\
	gap_lock.c		\
	gap_lock.h		\
	gap_navi_activtable.c	\
	gap_navi_activtable.h	\
	gap_match.c		\
	gap_match.h		\
	gap_onion_base.c	\
	gap_onion_base.h	\
	gap_pixelrgn.c		\
	gap_pixelrgn.h		\
	gap_pdb_calls.c		\
	gap_pdb_calls.h		\
	gap_pview_da.c		\
	gap_pview_da.h		\
	gap_thumbnail.c		\
	gap_thumbnail.h		\
	gap_timeconv.c		\
	gap_timeconv.h		\
	gap_stock.c		\
	gap_stock.h		\
	gap_xml_util.c		\
	gap_xml_util.h		\
	gap_vin.c		\
	gap_vin.h


MOVEPATH_SOURCES = \
	gap_bluebox.c		\
	gap_bluebox.h		\
	gap_mov_dialog.c	\
	gap_mov_dialog.h	\
	gap_mov_exec.c		\
	gap_mov_exec.h		\
	gap_mov_render.c	\
	gap_mov_render.h	\
	gap_mov_xml_par.c	\
	gap_mov_xml_par.h

libgimpgap_a_SOURCES = $(BASE_SOURCES)


libgapstory_a_SOURCES = $(BASE_SOURCES)	$(MOVEPATH_SOURCES) \
	gap_frame_fetcher.c		\
	gap_frame_fetcher.h		\
	gap_fmac_name.c		\
	gap_fmac_name.h		\
	gap_fmac_context.c	\
	gap_fmac_context.h	\
	gap_story_file.h		\
	gap_story_file.c		\
	gap_story_render_types.h	\
	gap_story_render_processor.h	\
	gap_story_render_processor.c	\
	gap_story_render_audio.h	\
	gap_story_render_audio.c	\
	gap_story_sox.h			\
	gap_story_sox.c			\
	gap_story_syntax.h		\
	gap_story_syntax.c

libexec_PROGRAMS = \
	gap_blend_fill		\
	gap_bluebox		\
	gap_edge		\
	gap_colormask		\
	gap_detail_tracking	\
	gap_plugins		\
	gap_movepath		\
	gap_filter		\
	gap_fmac		\
	gap_fmac_varying	\
	$(GAP_FRONTENDS)	\
	$(GAP_DECODE_MPLAYER_FRONTEND)	\
	gap_morph		\
	gap_name2layer		\
	gap_navigator_dialog	\
	gap_player		\
	gap_onion		\
	gap_opacity_exposure	\
	gap_storyboard		\
	$(GAP_VIDEO_EXTRACT)	\
	$(GAP_VIDEO_INDEX)	\
	gap_fg_matting		\
	gap_fire_pattern	\
	gap_water_pattern	\
	gap_wr_color_curve	\
	gap_wr_color_levels	\
	gap_wr_color_huesat	\
	gap_wr_color_balance	\
	gap_wr_trans		\
	gap_wr_resynth		\
	gap_wr_layermode	\
	gap_wr_desaturate       \
	gap_wr_opacity


gap_blend_fill_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_blend_fill_main.c   \
	gap_libgimpgap.h	

gap_bluebox_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_bluebox_main.c	\
	gap_bluebox.c		\
	gap_bluebox.h		\
	gap_libgimpgap.h	

gap_edge_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_edge_detection_main.c	\
	gap_edge_detection.c		\
	gap_edge_detection.h		\
	gap_edge_detection_dialog.c	\
	gap_edge_detection_dialog.h	\
	gap_libgimpgap.h	

gap_colormask_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_colormask_dialog.c	\
	gap_colormask_dialog.h 	\
	gap_colormask_main.c	\
	gap_libgimpgap.h	


gap_detail_tracking_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_detail_tracking_main.c	\
	gap_detail_align_exec.c	\
	gap_detail_align_exec.h	\
	gap_libgimpgap.h	

gap_plugins_SOURCES = \
	gap_base_ops.c		\
	gap_base_ops.h		\
	gap_bluebox.c		\
	gap_bluebox.h		\
	gap_dbbrowser_utils.c	\
	gap_dbbrowser_utils.h	\
	gap_filter_codegen.c	\
	gap_filter_pdb.c	\
	gap_filter_pdb.h	\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_main.c		\
	gap_mod_layer.c		\
	gap_mod_layer.h		\
	gap_mod_layer_dialog.c	\
	gap_mod_layer_dialog.h	\
	gap_navi_activtable.c	\
	gap_navi_activtable.h	\
	gap_range_ops.c		\
	gap_range_ops.h		\
	gap_resi_dialog.c	\
	gap_resi_dialog.h	\
	gap_split.c		\
	gap_split.h		\
	gap_libgimpgap.h	

gap_movepath_SOURCES =	$(MOVEPATH_SOURCES) \
	gap_base_ops.c		\
	gap_base_ops.h		\
	gap_mov_main.c		\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h	

gap_filter_SOURCES = \
	gap_dbbrowser_utils.c	\
	gap_dbbrowser_utils.h	\
	gap_filter.h		\
	gap_filter_codegen.c	\
	gap_filter_foreach.c	\
	gap_filter_iterators.c	\
	gap_filter_iterators.h	\
	gap_filter_main.c	\
	gap_filter_pdb.c	\
	gap_filter_pdb.h	\
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_fmac_context.c	\
	gap_fmac_context.h	\
	gap_frame_fetcher.c	\
	gap_frame_fetcher.h	\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h	

gap_fmac_SOURCES = \
	gap_dbbrowser_utils.c	\
	gap_dbbrowser_utils.h	\
	gap_filter.h		\
	gap_filter_codegen.c	\
	gap_filter_foreach.c	\
	gap_filter_iterators.c	\
	gap_filter_iterators.h	\
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_fmac_main.c	\
	gap_filter_pdb.c	\
	gap_filter_pdb.h	\
	gap_frame_fetcher.c	\
	gap_frame_fetcher.h	\
	gap_fmac_name.c		\
	gap_fmac_name.h		\
	gap_fmac_context.c	\
	gap_fmac_context.h	\
	gap_fmac_base.c		\
	gap_fmac_base.h		\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h	

gap_fmac_varying_SOURCES = \
	gap_filter.h		\
	gap_filter_iterators.c	\
	gap_filter_iterators.h	\
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_fmac_varying_main.c	\
	gap_filter_pdb.c	\
	gap_filter_pdb.h	\
	gap_frame_fetcher.c	\
	gap_frame_fetcher.h	\
	gap_fmac_name.c		\
	gap_fmac_name.h		\
	gap_fmac_context.c	\
	gap_fmac_context.h	\
	gap_fmac_base.c		\
	gap_fmac_base.h		\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h	

gap_frontends_SOURCES = \
	gap_decode_xanim.c	\
	gap_decode_xanim.h	\
	gap_frontends_main.c	\
	gap_mpege.c		\
	gap_mpege.h		\
	gap_libgimpgap.h	

gap_decode_mplayer_SOURCES = \
	gap_decode_mplayer_main.c	\
	gap_decode_mplayer.c	\
	gap_decode_mplayer.h	\
	gap_libgimpgap.h	

#  gap_morph_SOURCES: gap_mov_exec.h should be removed when finished coding
gap_morph_SOURCES = \
	gap_morph_main.c	\
	gap_morph_main.h	\
	gap_morph_exec.c	\
	gap_morph_exec.h	\
	gap_morph_shape.c	\
	gap_morph_shape.h	\
	gap_morph_dialog.c	\
	gap_morph_dialog.h	\
	gap_morph_tween_dialog.c	\
	gap_morph_tween_dialog.h	\
	gap_mov_dialog.h	\
	gap_mov_exec.h		\
	gap_libgimpgap.h	

gap_name2layer_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_name2layer_main.c	\
	gap_libgimpgap.h	

gap_navigator_dialog_SOURCES = \
	gap_navi_activtable.c	\
	gap_navi_activtable.h	\
	gap_navigator_dialog.c	\
	gap_libgimpgap.h	

gap_player_SOURCES = \
	gap_player_main.c	\
	gap_player_main.h	\
	gap_player_dialog.c	\
	gap_player_dialog.h	\
	gap_player_cache.c	\
	gap_player_cache.h	\
	$(GAP_SDL_AUDIO_EXTRASRC) \
	gap_audio_extract.c	\
	gap_audio_extract.h	\
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_libgapstory.h	\
	gap_libgimpgap.h	


gap_onion_SOURCES = \
	gap_onion_main.c	\
	gap_onion_main.h	\
	gap_onion_dialog.c	\
	gap_onion_dialog.h	\
	gap_onion_worker.c	\
	gap_onion_worker.h	\
	gap_libgimpgap.h	

gap_opacity_exposure_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_opacity_exposure_main.c   \
	gap_libgimpgap.h	


gap_storyboard_SOURCES = \
	gap_story_main.c	\
	gap_story_main.h	\
	gap_story_dialog.c	\
	gap_story_dialog.h	\
	gap_story_undo.c	\
	gap_story_undo.h	\
	gap_story_undo_types.h	\
	gap_story_vthumb.c	\
	gap_story_vthumb.h	\
	gap_story_section_properties.c	\
	gap_story_section_properties.h	\
	gap_story_properties.c	\
	gap_story_properties.h	\
	gap_story_att_trans_dlg.c \
	gap_story_att_trans_dlg.h \
	gap_audio_extract.c	\
	gap_audio_extract.h	\
	gap_player_main.h	\
	gap_player_dialog.c	\
	gap_player_dialog.h	\
	gap_player_cache.c	\
	gap_player_cache.h	\
	$(GAP_SDL_AUDIO_EXTRASRC) \
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_libgapstory.h	\
	gap_libgimpgap.h	

gap_video_extract_SOURCES = \
	gap_bluebox.c		\
	gap_bluebox.h		\
	gap_vex_main.c		\
	gap_vex_main.h		\
	gap_vex_exec.c		\
	gap_vex_exec.h		\
	gap_vex_dialog.c	\
	gap_vex_dialog.h	\
	gap_audio_extract.c	\
	gap_audio_extract.h	\
	gap_player_main.h	\
	gap_player_dialog.c	\
	gap_player_dialog.h	\
	gap_player_cache.c	\
	gap_player_cache.h	\
	$(GAP_SDL_AUDIO_EXTRASRC) \
	gap_drawable_vref_parasite.c	\
	gap_drawable_vref_parasite.h	\
	gap_libgapstory.h	\
	gap_libgimpgap.h	


gap_video_index_SOURCES = \
	gap_video_index_creator.c	\
	gap_libgapstory.h	\
	gap_libgimpgap.h	

gap_fg_matting_SOURCES = \
	gap_fg_matting_main.c	\
	gap_fg_matting_main.h	\
	gap_fg_matting_exec.c	\
	gap_fg_matting_exec.h	\
	gap_fg_matting.c	\
	gap_fg_matting.h	\
	gap_fg_matting_dialog.c	\
	gap_fg_matting_dialog.h	\
	gap_fg_from_sel_dialog.c	\
	gap_fg_from_sel_dialog.h	\
	gap_fg_regions.c	\
	gap_fg_regions.h	\
	gap_fg_tile_manager.h


gap_fire_pattern_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_fire_pattern.c	\
	gap_libgimpgap.h

gap_water_pattern_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_water_pattern.c	\
	gap_libgimpgap.h


gap_wr_opacity_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_wr_opacity.c	\
	gap_libgimpgap.h	

gap_wr_trans_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_wr_trans.c	\
	gap_libgimpgap.h	

gap_wr_layermode_SOURCES = \
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_wr_layermode.c	\
	gap_libgimpgap.h	

gap_wr_color_curve_SOURCES = \
	gap_wr_color_curve.c	\
	gap_libgimpgap.h	

gap_wr_color_levels_SOURCES = \
	gap_wr_color_levels.c	\
	gap_libgimpgap.h	

gap_wr_color_huesat_SOURCES = \
	gap_wr_color_huesat.c	\
	gap_libgimpgap.h

gap_wr_color_balance_SOURCES = \
	gap_wr_color_balance.c	\
	gap_libgimpgap.h

gap_wr_resynth_SOURCES = \
	gap_wr_resynth.c	\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h


gap_wr_desaturate_SOURCES = \
	gap_wr_desaturate.c	\
	gap_lastvaldesc.c	\
	gap_lastvaldesc.h	\
	gap_libgimpgap.h


if OS_WIN32
mwindows = -mwindows
endif

AM_CPPFLAGS = \
	-DGAPLIBDIR=\""$(GAPLIBDIR)"\"	\
	-DLOCALEDIR=\""$(LOCALEDIR)"\"

INCLUDES = \
	-I$(top_srcdir)	\
	-I$(top_srcdir)/libwavplayclient	\
	$(INC_LIBGAPBASE)	\
	$(INC_GAPVIDEOAPI)	\
	$(GAP_AUDIO_SDL_CFLAGS)	\
	$(GIMP_CFLAGS)	\
	-I$(includedir)


AM_LDFLAGS = $(mwindows)

LDADD = $(GIMP_LIBS)


gap_plugins_LDADD =          $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS) -lm
gap_movepath_LDADD =         $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_bluebox_LDADD =          $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_blend_fill_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_colormask_LDADD =        $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_detail_tracking_LDADD =  $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_edge_LDADD =             $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_filter_LDADD =           $(GAPVIDEOAPI) $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_fmac_LDADD =             $(GAPVIDEOAPI) $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_fmac_varying_LDADD =     $(GAPVIDEOAPI) $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_frontends_LDADD =        $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_decode_mplayer_LDADD =   $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_morph_LDADD =            $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS) -lm
gap_name2layer_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_navigator_dialog_LDADD = $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_player_LDADD =           $(GAPVIDEOAPI) $(GAP_AUDIO_LIBS) ${LIBGAPSTORY} $(LIBGAPBASE) $(GIMP_LIBS)
gap_onion_LDADD =            $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_opacity_exposure_LDADD = $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_storyboard_LDADD =       $(GAPVIDEOAPI) $(GAP_AUDIO_LIBS) ${LIBGAPSTORY} $(LIBGAPBASE) $(GIMP_LIBS)
gap_video_extract_LDADD =    $(GAPVIDEOAPI) $(GAP_AUDIO_LIBS) ${LIBGAPSTORY} $(LIBGAPBASE) $(GIMP_LIBS)
gap_video_index_LDADD =      $(GAPVIDEOAPI) $(LIBGAPSTORY) $(LIBGAPBASE)  $(GIMP_LIBS)
gap_fg_matting_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS) -lm
gap_fire_pattern_LDADD =     $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_water_pattern_LDADD =    $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_opacity_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_trans_LDADD =         $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_layermode_LDADD =     $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_color_curve_LDADD =   $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_color_levels_LDADD =  $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_color_huesat_LDADD =  $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_color_balance_LDADD = $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_resynth_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)
gap_wr_desaturate_LDADD =       $(LIBGIMPGAP)  $(LIBGAPBASE) $(GIMP_LIBS)


EXTRA_DIST = \
	README							\
	README_developers					\
	TESTPROT_iter_ALT					\
	gimplastvaldesc.c					\
	gimplastvaldesc.h					\
	gap_story_render_lossless.c				\
	iter_ALT/README_iter_subdirs				\
	iter_ALT/gen/plug_in_CML_explorer_iter_ALT.inc		\
	iter_ALT/gen/plug_in_alpha2color_iter_ALT.inc		\
	iter_ALT/gen/plug_in_blinds_iter_ALT.inc		\
	iter_ALT/gen/plug_in_borderaverage_iter_ALT.inc		\
	iter_ALT/gen/plug_in_checkerboard_iter_ALT.inc		\
	iter_ALT/gen/plug_in_color_map_iter_ALT.inc		\
	iter_ALT/gen/plug_in_colorify_iter_ALT.inc		\
	iter_ALT/gen/plug_in_cubism_iter_ALT.inc		\
	iter_ALT/gen/plug_in_destripe_iter_ALT.inc		\
	iter_ALT/gen/plug_in_diffraction_iter_ALT.inc		\
	iter_ALT/gen/plug_in_displace_iter_ALT.inc		\
	iter_ALT/gen/plug_in_edge_iter_ALT.inc			\
	iter_ALT/gen/plug_in_engrave_iter_ALT.inc		\
	iter_ALT/gen/plug_in_flarefx_iter_ALT.inc		\
	iter_ALT/gen/plug_in_fractal_trace_iter_ALT.inc		\
	iter_ALT/gen/plug_in_gfig_iter_ALT.inc			\
	iter_ALT/gen/plug_in_glasstile_iter_ALT.inc		\
	iter_ALT/gen/plug_in_grid_iter_ALT.inc			\
	iter_ALT/gen/plug_in_jigsaw_iter_ALT.inc		\
	iter_ALT/gen/plug_in_mblur_iter_ALT.inc			\
	iter_ALT/gen/plug_in_mosaic_iter_ALT.inc		\
	iter_ALT/gen/plug_in_newsprint_iter_ALT.inc		\
	iter_ALT/gen/plug_in_noisify_iter_ALT.inc		\
	iter_ALT/gen/plug_in_pixelize_iter_ALT.inc		\
	iter_ALT/gen/plug_in_randomize_hurl_iter_ALT.inc	\
	iter_ALT/gen/plug_in_randomize_pick_iter_ALT.inc	\
	iter_ALT/gen/plug_in_randomize_slur_iter_ALT.inc	\
	iter_ALT/gen/plug_in_ripple_iter_ALT.inc		\
	iter_ALT/gen/plug_in_scatter_hsv_iter_ALT.inc		\
	iter_ALT/gen/plug_in_sharpen_iter_ALT.inc		\
	iter_ALT/gen/plug_in_shift_iter_ALT.inc			\
	iter_ALT/gen/plug_in_spread_iter_ALT.inc		\
	iter_ALT/gen/plug_in_video_iter_ALT.inc			\
	iter_ALT/gen/plug_in_vpropagate_iter_ALT.inc		\
	iter_ALT/gen/plug_in_waves_iter_ALT.inc			\
	iter_ALT/gen/plug_in_whirl_pinch_iter_ALT.inc		\
	iter_ALT/gen/plug_in_wind_iter_ALT.inc			\
	iter_ALT/mod/plug_in_Twist_iter_ALT.inc			\
	iter_ALT/mod/plug_in_alienmap_iter_ALT.inc		\
	iter_ALT/mod/plug_in_applylens_iter_ALT.inc		\
	iter_ALT/mod/plug_in_bump_map_iter_ALT.inc		\
	iter_ALT/mod/plug_in_cartoon_iter_ALT.inc		\
	iter_ALT/mod/plug_in_colors_channel_mixer_iter_ALT.inc	\
	iter_ALT/mod/plug_in_convmatrix_iter_ALT.inc		\
	iter_ALT/mod/plug_in_depth_merge_iter_ALT.inc		\
	iter_ALT/mod/plug_in_despeckle_iter_ALT.inc		\
	iter_ALT/mod/plug_in_dog_iter_ALT.inc			\
	iter_ALT/mod/plug_in_emboss_iter_ALT.inc		\
	iter_ALT/mod/plug_in_exchange_iter_ALT.inc		\
	iter_ALT/mod/plug_in_flame_iter_ALT.inc			\
	iter_ALT/mod/plug_in_gauss_iter_ALT.inc			\
	iter_ALT/mod/plug_in_gimpressionist_iter_ALT.inc	\
	iter_ALT/mod/plug_in_lighting_iter_ALT.inc		\
	iter_ALT/mod/plug_in_map_object_iter_ALT.inc		\
	iter_ALT/mod/plug_in_maze_iter_ALT.inc			\
	iter_ALT/mod/plug_in_neon_iter_ALT.inc			\
	iter_ALT/mod/plug_in_nlfilt_iter_ALT.inc		\
	iter_ALT/mod/plug_in_nova_iter_ALT.inc			\
	iter_ALT/mod/plug_in_oilify_iter_ALT.inc		\
	iter_ALT/mod/plug_in_pagecurl_iter_ALT.inc		\
	iter_ALT/mod/plug_in_papertile_iter_ALT.inc		\
	iter_ALT/mod/plug_in_photocopy_iter_ALT.inc		\
	iter_ALT/mod/plug_in_plasma_iter_ALT.inc		\
	iter_ALT/mod/plug_in_polar_coords_iter_ALT.inc		\
	iter_ALT/mod/plug_in_retinex_iter_ALT.inc		\
	iter_ALT/mod/plug_in_sample_colorize_iter_ALT.inc	\
	iter_ALT/mod/plug_in_sinus_iter_ALT.inc			\
	iter_ALT/mod/plug_in_softglow_iter_ALT.inc		\
	iter_ALT/mod/plug_in_solid_noise_iter_ALT.inc		\
	iter_ALT/mod/plug_in_sparkle_iter_ALT.inc		\
	iter_ALT/mod/plug_in_alienmap2_iter_ALT.inc		\
	iter_ALT/mod/plug_in_apply_canvas_iter_ALT.inc		\
	iter_ALT/mod/plug_in_colortoalpha_iter_ALT.inc		\
	iter_ALT/mod/plug_in_deinterlace_iter_ALT.inc		\
	iter_ALT/mod/plug_in_illusion_iter_ALT.inc		\
	iter_ALT/mod/plug_in_lic_iter_ALT.inc			\
	iter_ALT/mod/plug_in_sel_gauss_iter_ALT.inc		\
	iter_ALT/mod/plug_in_small_tiles_iter_ALT.inc		\
	iter_ALT/mod/plug_in_sobel_iter_ALT.inc			\
	iter_ALT/mod/plug_in_unsharp_mask_iter_ALT.inc		\
	iter_ALT/old/plug_in_CentralReflection_iter_ALT.inc	\
	iter_ALT/old/plug_in_anamorphose_iter_ALT.inc		\
	iter_ALT/old/plug_in_blur2_iter_ALT.inc			\
	iter_ALT/old/plug_in_colorify_iter_ALT.inc		\
	iter_ALT/old/plug_in_encript_iter_ALT.inc		\
	iter_ALT/old/plug_in_figures_iter_ALT.inc		\
	iter_ALT/old/plug_in_gflare_iter_ALT.inc		\
	iter_ALT/old/plug_in_holes_iter_ALT.inc			\
	iter_ALT/old/plug_in_julia_iter_ALT.inc			\
	iter_ALT/old/plug_in_magic_eye_iter_ALT.inc		\
	iter_ALT/old/plug_in_mandelbrot_iter_ALT.inc		\
	iter_ALT/old/plug_in_randomize_iter_ALT.inc		\
	iter_ALT/old/plug_in_refract_iter_ALT.inc		\
	iter_ALT/old/plug_in_struc_iter_ALT.inc			\
	iter_ALT/old/plug_in_tileit_iter_ALT.inc		\
	iter_ALT/old/plug_in_universal_filter_iter_ALT.inc	\
	iter_ALT/old/plug_in_warp_iter_ALT.inc			\
	$(scriptdata_DATA)
