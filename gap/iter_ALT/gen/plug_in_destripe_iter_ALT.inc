/* ----------------------------------------------------------------------
 * p_plug_in_destripe_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_destripe_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_destripe_Vals 
    {
      gboolean      histogram;
      gint          avg_width;
      gboolean preview;
    } t_plug_in_destripe_Vals; 

    t_plug_in_destripe_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_destripe_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_destripe_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_destripe_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-destripe-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-destripe-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_destripe_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_destripe_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gboolean(&buf.histogram, buf_from->histogram, buf_to->histogram, total_steps, current_step);
    p_delta_gint(&buf.avg_width, buf_from->avg_width, buf_to->avg_width, total_steps, current_step);

    buf.preview = FALSE;
    gimp_set_data("plug-in-destripe", &buf, sizeof(buf)); 

    return 0; /* OK */
}
