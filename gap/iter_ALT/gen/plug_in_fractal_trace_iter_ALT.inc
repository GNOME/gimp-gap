/* ----------------------------------------------------------------------
 * p_plug_in_fractal_trace_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_fractal_trace_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_fractal_trace_Vals 
    {
      gdouble   x1;
      gdouble   x2;
      gdouble   y1;
      gdouble   y2;
      gint32    depth;
      gint32    outside_type;
    } t_plug_in_fractal_trace_Vals; 


    t_plug_in_fractal_trace_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_fractal_trace_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_fractal_trace_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_fractal_trace_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-fractal-trace-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-fractal-trace-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_fractal_trace_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_fractal_trace_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gdouble(&buf.x1, buf_from->x1, buf_to->x1, total_steps, current_step);
    p_delta_gdouble(&buf.x2, buf_from->x2, buf_to->x2, total_steps, current_step);
    p_delta_gdouble(&buf.y1, buf_from->y1, buf_to->y1, total_steps, current_step);
    p_delta_gdouble(&buf.y2, buf_from->y2, buf_to->y2, total_steps, current_step);
    p_delta_gint32(&buf.depth, buf_from->depth, buf_to->depth, total_steps, current_step);
    p_delta_gint32(&buf.outside_type, buf_from->outside_type, buf_to->outside_type, total_steps, current_step);

    gimp_set_data("plug-in-fractal-trace", &buf, sizeof(buf)); 

    return 0; /* OK */
}
