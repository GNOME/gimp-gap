/* ----------------------------------------------------------------------
 * p_plug_in_colors_channel_mixer_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_colors_channel_mixer_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef enum
    {
      CM_RED_CHANNEL,
      CM_GREEN_CHANNEL,
      CM_BLUE_CHANNEL
    } CmModeType;

    typedef struct t_plug_in_colors_channel_mixer_Vals 
    {
      t_channel_mixer_ch_type  red;
      t_channel_mixer_ch_type  green;
      t_channel_mixer_ch_type  blue;
      t_channel_mixer_ch_type  black;

      gboolean                 monochrome_flag;
      gboolean                 preserve_luminosity_flag;

      CmModeType               output_channel;
      gboolean                 preview;


      /* other parts are not relevant for last_values runmode */
      GtkAdjustment *red_data;
      GtkAdjustment *green_data;
      GtkAdjustment *blue_data;

      GtkWidget     *combo;

      CmModeType     old_output_channel;

      GtkWidget     *monochrome_toggle;
      GtkWidget     *preserve_luminosity_toggle;
    } t_plug_in_colors_channel_mixer_Vals; 

    t_plug_in_colors_channel_mixer_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_colors_channel_mixer_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_colors_channel_mixer_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_colors_channel_mixer_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-colors-channel-mixer-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-colors-channel-mixer-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_colors_channel_mixer_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_colors_channel_mixer_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_channel_mixer_ch_type(&buf.red, &buf_from->red, &buf_to->red, total_steps, current_step);
    p_delta_channel_mixer_ch_type(&buf.green, &buf_from->green, &buf_to->green, total_steps, current_step);
    p_delta_channel_mixer_ch_type(&buf.blue, &buf_from->blue, &buf_to->blue, total_steps, current_step);
    p_delta_channel_mixer_ch_type(&buf.black, &buf_from->black, &buf_to->black, total_steps, current_step);

    p_delta_gboolean(&buf.monochrome_flag, buf_from->monochrome_flag, buf_to->monochrome_flag, total_steps, current_step);
    p_delta_gboolean(&buf.preserve_luminosity_flag, buf_from->preserve_luminosity_flag, buf_to->preserve_luminosity_flag, total_steps, current_step);

    buf.preview = FALSE;

    gimp_set_data("plug-in-colors-channel-mixer", &buf, sizeof(buf)); 

    return 0; /* OK */
}
