/* ----------------------------------------------------------------------
 * p_plug_in_blinds_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_blinds_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_blinds_Vals 
    {
      gint     angledsp;
      gint     numsegs;
      GimpOrientationType  orientation;
      gboolean bg_trans;
    } t_plug_in_blinds_Vals;
    
 
    t_plug_in_blinds_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_blinds_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_blinds_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_blinds_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-blinds-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-blinds-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_blinds_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_blinds_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));


    p_delta_gint(&buf.angledsp, buf_from->angledsp, buf_to->angledsp, total_steps, current_step);
    p_delta_gint(&buf.numsegs, buf_from->numsegs, buf_to->numsegs, total_steps, current_step);
    p_delta_GimpOrientationType(&buf.orientation, &buf_from->orientation, &buf_to->orientation, total_steps, current_step);
    p_delta_gboolean(&buf.bg_trans, buf_from->bg_trans, buf_to->bg_trans, total_steps, current_step);
    
    gimp_set_data("plug-in-blinds", &buf, sizeof(buf)); 

    return 0; /* OK */
}
