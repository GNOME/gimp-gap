/* ----------------------------------------------------------------------
 * p_plug_in_newsprint_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_newsprint_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_newsprint_Vals 
    {
      gint      cell_width;
      gint      colorspace;
      gint      k_pullout;
      gdouble   gry_ang;
      gint      gry_spotfn;
      gdouble   red_ang;
      gint      red_spotfn;
      gdouble   grn_ang;
      gint      grn_spotfn;
      gdouble   blu_ang;
      gint      blu_spotfn;
      gint      oversample;
    } t_plug_in_newsprint_Vals; 

    t_plug_in_newsprint_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_newsprint_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_newsprint_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_newsprint_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-newsprint-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-newsprint-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_newsprint_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_newsprint_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gint(&buf.cell_width, buf_from->cell_width, buf_to->cell_width, total_steps, current_step);
    p_delta_gint(&buf.colorspace, buf_from->colorspace, buf_to->colorspace, total_steps, current_step);
    p_delta_gint(&buf.k_pullout, buf_from->k_pullout, buf_to->k_pullout, total_steps, current_step);
    p_delta_gdouble(&buf.gry_ang, buf_from->gry_ang, buf_to->gry_ang, total_steps, current_step);
    p_delta_gint(&buf.gry_spotfn, buf_from->gry_spotfn, buf_to->gry_spotfn, total_steps, current_step);
    p_delta_gdouble(&buf.red_ang, buf_from->red_ang, buf_to->red_ang, total_steps, current_step);
    p_delta_gint(&buf.red_spotfn, buf_from->red_spotfn, buf_to->red_spotfn, total_steps, current_step);
    p_delta_gdouble(&buf.grn_ang, buf_from->grn_ang, buf_to->grn_ang, total_steps, current_step);
    p_delta_gint(&buf.grn_spotfn, buf_from->grn_spotfn, buf_to->grn_spotfn, total_steps, current_step);
    p_delta_gdouble(&buf.blu_ang, buf_from->blu_ang, buf_to->blu_ang, total_steps, current_step);
    p_delta_gint(&buf.blu_spotfn, buf_from->blu_spotfn, buf_to->blu_spotfn, total_steps, current_step);
    p_delta_gint(&buf.oversample, buf_from->oversample, buf_to->oversample, total_steps, current_step);

    gimp_set_data("plug-in-newsprint", &buf, sizeof(buf)); 

    return 0; /* OK */
}
