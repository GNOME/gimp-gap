/* ----------------------------------------------------------------------
 * p_plug_in_ripple_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_ripple_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_ripple_Vals 
    {
      gint                   period;
      gint                   amplitude;
      GimpOrientationType    orientation;
      gint                   edges;
      gint                   waveform;
      gboolean               antialias;
      gboolean               tile;
      gint                   phase_shift;
    } t_plug_in_ripple_Vals; 

    t_plug_in_ripple_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_ripple_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_ripple_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_ripple_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-ripple-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-ripple-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_ripple_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_ripple_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gint(&buf.period, buf_from->period, buf_to->period, total_steps, current_step);
    p_delta_gint(&buf.amplitude, buf_from->amplitude, buf_to->amplitude, total_steps, current_step);
    p_delta_GimpOrientationType(&buf.orientation, &buf_from->orientation, &buf_to->orientation, total_steps, current_step);
    p_delta_gint(&buf.edges, buf_from->edges, buf_to->edges, total_steps, current_step);
    p_delta_gint(&buf.waveform, buf_from->waveform, buf_to->waveform, total_steps, current_step);
    p_delta_gboolean(&buf.antialias, buf_from->antialias, buf_to->antialias, total_steps, current_step);
    p_delta_gboolean(&buf.tile, buf_from->tile, buf_to->tile, total_steps, current_step);
    p_delta_gint(&buf.phase_shift, buf_from->phase_shift, buf_to->phase_shift, total_steps, current_step);

    gimp_set_data("plug-in-ripple", &buf, sizeof(buf)); 

    return 0; /* OK */
}
