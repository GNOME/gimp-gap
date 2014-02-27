/* ----------------------------------------------------------------------
 * p_plug_in_mblur_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_mblur_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_mblur_Vals 
    {
      gint32    mblur_type;
      gint32    length;
      gint32    angle;
      gdouble   center_x;
      gdouble   center_y;
      gboolean  blur_outward;

    } t_plug_in_mblur_Vals; 

    t_plug_in_mblur_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_mblur_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_mblur_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_mblur_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-mblur-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-mblur-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_mblur_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_mblur_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gint32(&buf.mblur_type, buf_from->mblur_type, buf_to->mblur_type, total_steps, current_step);
    p_delta_gint32(&buf.length, buf_from->length, buf_to->length, total_steps, current_step);
    p_delta_gint32(&buf.angle, buf_from->angle, buf_to->angle, total_steps, current_step);

    p_delta_gdouble(&buf.center_x, buf_from->center_x, buf_to->center_x, total_steps, current_step);
    p_delta_gdouble(&buf.center_y, buf_from->center_y, buf_to->center_y, total_steps, current_step);

    p_delta_gboolean(&buf.blur_outward, buf_from->blur_outward, buf_to->blur_outward, total_steps, current_step);
 
    gimp_set_data("plug-in-mblur", &buf, sizeof(buf)); 

    return 0; /* OK */
}
