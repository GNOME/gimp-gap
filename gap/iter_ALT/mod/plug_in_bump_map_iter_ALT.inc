/* ----------------------------------------------------------------------
 * p_plug_in_bump_map_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_bump_map_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
    typedef struct t_plug_in_bump_map_Vals 
    {
      gint32      bumpmap_id;
      gdouble     azimuth;
      gdouble     elevation;
      gint        depth;
      gint        xofs;
      gint        yofs;
      gint        waterlevel;
      gint        ambient;
      gboolean    compensate;
      gboolean    invert;
      gint        type;
      gboolean    tiled;
    } t_plug_in_bump_map_Vals; 




    t_plug_in_bump_map_Vals  buf, *buf_from, *buf_to; 

    if(len_struct != sizeof(t_plug_in_bump_map_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_bump_map_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_bump_map_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-bump-map-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-bump-map-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_bump_map_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_bump_map_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_drawable(&buf.bumpmap_id, buf_from->bumpmap_id, buf_to->bumpmap_id, total_steps, current_step);
    p_delta_gdouble(&buf.azimuth, buf_from->azimuth, buf_to->azimuth, total_steps, current_step);
    p_delta_gdouble(&buf.elevation, buf_from->elevation, buf_to->elevation, total_steps, current_step);
    p_delta_gint(&buf.depth, buf_from->depth, buf_to->depth, total_steps, current_step);
    p_delta_gint(&buf.xofs, buf_from->xofs, buf_to->xofs, total_steps, current_step);
    p_delta_gint(&buf.yofs, buf_from->yofs, buf_to->yofs, total_steps, current_step);
    p_delta_gint(&buf.waterlevel, buf_from->waterlevel, buf_to->waterlevel, total_steps, current_step);
    p_delta_gint(&buf.ambient, buf_from->ambient, buf_to->ambient, total_steps, current_step);
    p_delta_gint(&buf.type, buf_from->type, buf_to->type, total_steps, current_step);

    gimp_set_data("plug-in-bump-map", &buf, sizeof(buf)); 

    return 0; /* OK */
}
