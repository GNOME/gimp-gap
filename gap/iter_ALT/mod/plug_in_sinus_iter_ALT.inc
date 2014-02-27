/* ----------------------------------------------------------------------
 * p_plug_in_sinus_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_sinus_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_sinus_Vals 
    {
      gdouble   scalex;
      gdouble   scaley;
      gdouble   cmplx;
      gdouble   blend_power;
      guint32   seed;
      gint      tiling;
      glong     perturbation;
      glong     colorization;
      glong     colors;
      GimpRGB   col1;
      GimpRGB   col2;
      gboolean  random_seed;
    } t_plug_in_sinus_Vals;

    t_plug_in_sinus_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_sinus_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_sinus_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_sinus_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-sinus-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-sinus-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_sinus_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_sinus_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gdouble(&buf.scalex, buf_from->scalex, buf_to->scalex, total_steps, current_step);
    p_delta_gdouble(&buf.scaley, buf_from->scaley, buf_to->scaley, total_steps, current_step);
    p_delta_gdouble(&buf.cmplx, buf_from->cmplx, buf_to->cmplx, total_steps, current_step);
    p_delta_gdouble(&buf.blend_power, buf_from->blend_power, buf_to->blend_power, total_steps, current_step);
    p_delta_guint32(&buf.seed, buf_from->seed, buf_to->seed, total_steps, current_step);
    p_delta_gint(&buf.tiling, buf_from->tiling, buf_to->tiling, total_steps, current_step);
    p_delta_long(&buf.perturbation, buf_from->perturbation, buf_to->perturbation, total_steps, current_step);
    p_delta_long(&buf.colorization, buf_from->colorization, buf_to->colorization, total_steps, current_step);
    p_delta_long(&buf.colors, buf_from->colors, buf_to->colors, total_steps, current_step);
    p_delta_GimpRGB(&buf.col1, &buf_from->col1, &buf_to->col1, total_steps, current_step);
    p_delta_GimpRGB(&buf.col2, &buf_from->col2, &buf_to->col2, total_steps, current_step);
    p_delta_gboolean(&buf.random_seed, buf_from->random_seed, buf_to->random_seed, total_steps, current_step);

    gimp_set_data("plug-in-sinus", &buf, sizeof(buf)); 

    return 0; /* OK */
}
