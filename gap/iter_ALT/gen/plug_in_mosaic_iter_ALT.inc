/* ----------------------------------------------------------------------
 * p_plug_in_mosaic_iter_ALT 
 * ----------------------------------------------------------------------
 */
gint p_plug_in_mosaic_iter_ALT(GimpRunMode run_mode, gint32 total_steps, gdouble current_step, gint32 len_struct) 
{
  typedef enum
  {
    SQUARES   = 0,
    HEXAGONS  = 1,
    OCTAGONS  = 2,
    TRIANGLES = 3
  } t_plug_in_mosaic_TileType;

    typedef struct t_plug_in_mosaic_Vals 
    {
      gdouble   tile_size;
      gdouble   tile_height;
      gdouble   tile_spacing;
      gdouble   tile_neatness;
      long      tile_allow_split;
      gdouble   light_dir;
      gdouble   color_variation;
      gboolean  antialiasing;
      gint      color_averaging;
      t_plug_in_mosaic_TileType      tile_type;
      gint      tile_surface;
      gint      grout_color;
    } t_plug_in_mosaic_Vals; 

    t_plug_in_mosaic_Vals  buf, *buf_from, *buf_to; 
    
    gboolean l_boolean_switch;
    

    if(len_struct != sizeof(t_plug_in_mosaic_Vals)) 
    {
      fprintf(stderr, "ERROR: p_plug_in_mosaic_iter_ALT  stored Data missmatch in size %d != %d\n",   
                       (int)len_struct, sizeof(t_plug_in_mosaic_Vals) ); 
      return -1;  /* ERROR */ 
    }

    gimp_get_data("plug-in-mosaic-ITER-FROM", g_plugin_data_from); 
    gimp_get_data("plug-in-mosaic-ITER-TO",   g_plugin_data_to); 

    buf_from = (t_plug_in_mosaic_Vals *)&g_plugin_data_from[0]; 
    buf_to   = (t_plug_in_mosaic_Vals *)&g_plugin_data_to[0]; 
    memcpy(&buf, buf_from, sizeof(buf));

    p_delta_gdouble(&buf.tile_size, buf_from->tile_size, buf_to->tile_size, total_steps, current_step);
    p_delta_gdouble(&buf.tile_height, buf_from->tile_height, buf_to->tile_height, total_steps, current_step);
    p_delta_gdouble(&buf.tile_spacing, buf_from->tile_spacing, buf_to->tile_spacing, total_steps, current_step);
    p_delta_gdouble(&buf.tile_neatness, buf_from->tile_neatness, buf_to->tile_neatness, total_steps, current_step);
    p_delta_long(&buf.tile_allow_split, buf_from->tile_allow_split, buf_to->tile_allow_split, total_steps, current_step);
    p_delta_gdouble(&buf.light_dir, buf_from->light_dir, buf_to->light_dir, total_steps, current_step);
    p_delta_gdouble(&buf.color_variation, buf_from->color_variation, buf_to->color_variation, total_steps, current_step);
    p_delta_gboolean(&buf.antialiasing, buf_from->antialiasing, buf_to->antialiasing, total_steps, current_step);

    p_delta_gboolean(&l_boolean_switch, FALSE, TRUE, total_steps, current_step);

    /* enum is not iterated, just pick the other value when its time to switch */
    buf.tile_type = (l_boolean_switch) ? buf_to->tile_type : buf_from->tile_type;

    /* the follwning gint values are flags and therefore handled as gboolean types */
    buf.color_averaging = (l_boolean_switch) ? buf_to->color_averaging : buf_from->color_averaging;
    buf.tile_surface = (l_boolean_switch) ? buf_to->tile_surface : buf_from->tile_surface;
    buf.grout_color = (l_boolean_switch) ? buf_to->grout_color : buf_from->grout_color;

    gimp_set_data("plug-in-mosaic", &buf, sizeof(buf)); 

    return 0; /* OK */
}
