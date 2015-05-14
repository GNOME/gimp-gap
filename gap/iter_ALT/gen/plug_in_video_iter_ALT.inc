/* ----------------------------------------------------------------------
 * p_plug_in_video_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_video_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_video_Vals 
    {
      gint      pattern_number;
      gint      additive;
      gint      rotated;
    } t_plug_in_video_Vals; 

    t_plug_in_video_Vals  buf, *buf_from, *buf_to; 

    gboolean l_boolean_switch;

    if(len_struct != sizeof(t_plug_in_video_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_video_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_video_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-video-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-video-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_video_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_video_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gint(&buf.pattern_number, buf_from->pattern_number, buf_to->pattern_number, total_steps, current_step);

    p_delta_gboolean(&l_boolean_switch, FALSE, TRUE, total_steps, current_step);
    buf.additive = (l_boolean_switch) ? buf_to->additive : buf_from->additive;
    buf.rotated = (l_boolean_switch) ? buf_to->rotated : buf_from->rotated;


    gimp_set_data("plug-in-video", &buf, sizeof(buf)); 

    return 0; /* OK */
}
