void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
  MetaCompScreen *info;
  MetaDisplay    *display       = meta_screen_get_display (screen);
  Display        *xdisplay      = meta_display_get_xdisplay (display);
  Window          xwin;
  gint            width, height;

  /* Check if the screen is already managed */
  if (meta_screen_get_compositor_data (screen))
    return;

  info = g_new0 (MetaCompScreen, 1);
  /*
   * We use an empty input region for Clutter as a default because that allows
   * the user to interact with all the windows displayed on the screen.
   * We have to initialize info->pending_input_region to an empty region explicitly, 
   * because None value is used to mean that the whole screen is an input region.
   */
  info->pending_input_region = XFixesCreateRegion (xdisplay, NULL, 0);

  info->screen = screen;

  meta_screen_set_compositor_data (screen, info);

  info->output = None;
  info->windows = NULL;

  meta_screen_set_cm_selection (screen);

  info->stage = clutter_stage_new ();

  clutter_threads_add_repaint_func_full (CLUTTER_REPAINT_FLAGS_POST_PAINT,
                                         after_stage_paint,
                                         info, NULL);

  clutter_stage_set_sync_delay (CLUTTER_STAGE (info->stage), META_SYNC_DELAY);

  meta_screen_get_size (screen, &width, &height);
  clutter_actor_realize (info->stage);

  xwin = clutter_x11_get_stage_window (CLUTTER_STAGE (info->stage));

  XResizeWindow (xdisplay, xwin, width, height);

  {
    long event_mask;
    unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
    XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
    XWindowAttributes attr;

    meta_core_add_old_event_mask (xdisplay, xwin, &mask);

    XISetMask (mask.mask, XI_KeyPress);
    XISetMask (mask.mask, XI_KeyRelease);
    XISetMask (mask.mask, XI_ButtonPress);
    XISetMask (mask.mask, XI_ButtonRelease);
    XISetMask (mask.mask, XI_Enter);
    XISetMask (mask.mask, XI_Leave);
    XISetMask (mask.mask, XI_FocusIn);
    XISetMask (mask.mask, XI_FocusOut);
    XISetMask (mask.mask, XI_Motion);
    XISelectEvents (xdisplay, xwin, &mask, 1);

    event_mask = ExposureMask | PropertyChangeMask | StructureNotifyMask;
    if (XGetWindowAttributes (xdisplay, xwin, &attr))
      event_mask |= attr.your_event_mask;

    XSelectInput (xdisplay, xwin, event_mask);
  }

  info->window_group = meta_window_group_new (screen);
  info->top_window_group = meta_window_group_new (screen);
  info->overlay_group = clutter_actor_new ();

  clutter_actor_add_child (info->stage, info->window_group);
  clutter_actor_add_child (info->stage, info->top_window_group);
  clutter_actor_add_child (info->stage, info->overlay_group);

  info->plugin_mgr = meta_plugin_manager_new (screen);

  /*
   * Delay the creation of the overlay window as long as we can, to avoid
   * blanking out the screen. This means that during the plugin loading, the
   * overlay window is not accessible; if the plugin needs to access it
   * directly, it should hook into the "show" signal on stage, and do
   * its stuff there.
   */
  info->output = get_output_window (screen);
  XReparentWindow (xdisplay, xwin, info->output, 0, 0);

 /* Make sure there isn't any left-over output shape on the 
  * overlay window by setting the whole screen to be an
  * output region.
  * 
  * Note: there doesn't seem to be any real chance of that
  *  because the X server will destroy the overlay window
  *  when the last client using it exits.
  */
  XFixesSetWindowShapeRegion (xdisplay, info->output, ShapeBounding, 0, 0, None);

  do_set_stage_input_region (screen, info->pending_input_region);
  if (info->pending_input_region != None)
    {
      XFixesDestroyRegion (xdisplay, info->pending_input_region);
      info->pending_input_region = None;
    }

  clutter_actor_show (info->overlay_group);

  /* Map overlay window before redirecting windows offscreen so we catch their
   * contents until we show the stage.
   */
  XMapWindow (xdisplay, info->output);

  redirect_windows (compositor, screen);
}
