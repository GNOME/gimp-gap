/* ----------------------------------------------------------------------
 * p_plug_in_applylens_iter_ALT 
 * ----------------------------------------------------------------------
 */
static gint p_plug_in_applylens_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_applylens_Vals 
    {
      gdouble   refraction;
      gboolean  keep_surr;
      gboolean  use_bkgr;
      gboolean  set_transparent;
    } t_plug_in_applylens_Vals; 


  gdouble  refraction;


    t_plug_in_applylens_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_applylens_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_applylens_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_applylens_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-applylens-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-applylens-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_applylens_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_applylens_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gdouble(&buf.refraction, buf_from->refraction, buf_to->refraction, total_steps, current_step);
    p_delta_gboolean(&buf.keep_surr, buf_from->keep_surr, buf_to->keep_surr, total_steps, current_step);
    p_delta_gboolean(&buf.use_bkgr, buf_from->use_bkgr, buf_to->use_bkgr, total_steps, current_step);
    p_delta_gboolean(&buf.set_transparent, buf_from->set_transparent, buf_to->set_transparent, total_steps, current_step);

    gimp_set_data("plug-in-applylens", &buf, sizeof(buf)); 

    return 0; /* OK */
}
