/* ----------------------------------------------------------------------
 * p_plug_in_dog_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_dog_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_dog_Vals 
    {
      gdouble  inner;
      gdouble  outer;
      gboolean normalize;
      gboolean invert;
    } t_plug_in_dog_Vals; 

    t_plug_in_dog_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_dog_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_dog_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_dog_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-dog-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-dog-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_dog_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_dog_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gdouble(&buf.inner, buf_from->inner, buf_to->inner, total_steps, current_step);
    p_delta_gdouble(&buf.outer, buf_from->outer, buf_to->outer, total_steps, current_step);
    p_delta_gboolean(&buf.normalize, buf_from->normalize, buf_to->normalize, total_steps, current_step);
    p_delta_gboolean(&buf.invert, buf_from->invert, buf_to->invert, total_steps, current_step);


    gimp_set_data("plug-in-dog", &buf, sizeof(buf)); 

    return 0; /* OK */
}
