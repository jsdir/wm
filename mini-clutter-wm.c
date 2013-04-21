#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/XInput2.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <stdlib.h>
//#include <gdk/gdk.h>
//#include <gtk/gtk.h>

#define RECTS 0

int ii = 0;
Display *dpy;
Window root, overlay, stage_win, input;
ClutterActor *stage;
int stage_w, stage_h;
GType texture_pixmap_type;

GPollFD event_poll_fd;

ClutterActor *rects[RECTS];

void move_rect(ClutterActor *rect) {
    clutter_actor_animate (rect,
        CLUTTER_EASE_OUT_BACK,
        (rand() / (float)RAND_MAX) * 400 + 40000,
        "x", (float)(rand() % 1400),
        "y", (float)(rand() % 800),
        //"width", (float)(rand() % 600),
        //"height", (float)(rand() % 400),
        "rotation-angle-z", (float)(rand() % 360),
        NULL);
}

void on_stage_button_press (ClutterStage *stage, ClutterEvent *event, gpointer data) {
    int index;
    for (index = 0; index < RECTS; index++)
    {
        move_rect(rects[index]);
    }
}

/*static GdkFilterReturn
gdk_event_filter (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
    g_print("hellogdk");
}*/
static void
window_position_changed (ClutterActor *tex, GParamSpec *pspec, gpointer unused)
{
    gint x, y, window_x, window_y;
    g_object_get (tex, "x", &x, "y", &y, "window-x", &window_x,
                  "window-y", &window_y, NULL);
    if (x == window_x && y == window_y)
        return;
    
    clutter_actor_set_position (tex, window_x, window_y);
}

static void
window_mapped_changed (ClutterActor *tex, GParamSpec *pspec, gpointer unused)
{
    gint mapped;
    g_object_get (tex, "mapped", &mapped, NULL);

    if (mapped){
        clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);
        g_print("staging");
        //clutter_actor_set_size(tex, 1000, 1000);
        clutter_actor_show (tex);
        window_position_changed (tex, NULL, NULL);
    } else {
        clutter_container_remove_actor (CLUTTER_CONTAINER (stage), tex);
    }
}

static void
window_destroyed (ClutterActor *tex, GParamSpec *pspec, gpointer unused)
{
    g_print ("window destroyed\n");
}

static void
window_created (Window w)
{   
    ii++;
    g_print("A new window :D\n");
    XWindowAttributes attr;    
    ClutterActor *tex;

    if (w == overlay)
        return;

    XGetWindowAttributes (dpy, w, &attr);
    if (attr.class == InputOnly)
        return;
    
    tex = g_object_new (texture_pixmap_type, "window", w,
                        "automatic-updates", TRUE, NULL);

    g_signal_connect (tex, "notify::mapped", G_CALLBACK (window_mapped_changed), NULL);
    g_signal_connect (tex, "notify::destroyed", G_CALLBACK (window_destroyed), NULL);
    g_signal_connect (tex, "notify::window-x", G_CALLBACK (window_position_changed), NULL);
    g_signal_connect (tex, "notify::window-y", G_CALLBACK (window_position_changed), NULL);
    
    if (ii > 2)
    {
        clutter_actor_set_position (tex, 40, 40);
        clutter_container_add_actor (CLUTTER_CONTAINER (stage), tex);
        //g_print(w.title);
        clutter_actor_show (tex);
        move_rect(tex);
    }
    
    {
        gint mapped, destroyed;
        g_object_get (tex, "mapped", &mapped, "destroyed", &destroyed, NULL);
        
        if (mapped)
            window_mapped_changed (tex, NULL, NULL);
        if (destroyed)
            window_destroyed (tex, NULL, NULL);
    }
}

static ClutterX11FilterReturn
event_filter (XEvent *ev, ClutterEvent *cev, gpointer unused)
{
    switch (ev->type) {
    case CreateNotify:
        window_created (ev->xcreatewindow.window);
        return CLUTTER_X11_FILTER_REMOVE;

    case KeyPress:
        //if (handle_keypress (ev->xkey.window, ev->xkey.state, ev->xkey.keycode))
            //return CLUTTER_X11_FILTER_REMOVE;
        //else
            return CLUTTER_X11_FILTER_CONTINUE;

    case ConfigureNotify:
    case MapNotify:
    case UnmapNotify:
    case DestroyNotify:
    case ClientMessage:
    case PropertyNotify:
    case ButtonPress:
    case EnterNotify:
    case LeaveNotify:
    case FocusIn:
    case KeyRelease:
    case VisibilityNotify:
    case ColormapNotify:
    case MappingNotify:
    case MotionNotify:
    case SelectionNotify:
    default:
        return CLUTTER_X11_FILTER_CONTINUE;
    }
}

void
meta_core_add_old_event_mask (Display     *xdisplay,
                              Window       xwindow,
                              XIEventMask *mask)
{
  XIEventMask *prev;
  gint n_masks, i, j;

  prev = XIGetSelectedEvents (xdisplay, xwindow, &n_masks);

  for (i = 0; i < n_masks; i++)
    {
      if (prev[i].deviceid != XIAllMasterDevices)
        continue;

      for (j = 0; j < MIN (mask->mask_len, prev[i].mask_len); j++)
        mask->mask[j] |= prev[i].mask[j];
    }

  XFree (prev);
}

int main (int argc, char *argv[])
{
    //clutter_x11_disable_event_retrieval ();
    int r = clutter_init(NULL, NULL);

    dpy = clutter_x11_get_default_display ();
    root = DefaultRootWindow (dpy);

    XserverRegion pending_input_region = XFixesCreateRegion (dpy, NULL, 0);
    XCompositeRedirectSubwindows (dpy, root, CompositeRedirectAutomatic);
    XSelectInput (dpy, root, SubstructureNotifyMask);

    //gtk_init(&argc, &argv);
    //gdk_window_add_filter (NULL, gdk_event_filter, NULL);
    clutter_x11_add_filter (event_filter, NULL);

    //if (getenv("NO_TFP"))
        texture_pixmap_type = CLUTTER_X11_TYPE_TEXTURE_PIXMAP;
    //else
    //  texture_pixmap_type = CLUTTER_GLX_TYPE_TEXTURE_PIXMAP;

    ///////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////

    overlay = XCompositeGetOverlayWindow (dpy, root);

    ///////////////////////////////////////////////////////////////


    ClutterColor color;
    int index,
        width, 
        height;

    stage = clutter_stage_new();
    //clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
    //clutter_stage_set_sync_delay (CLUTTER_STAGE (stage), 2); //ms delay
    clutter_actor_realize(stage);

    stage_win = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

    width  = XDisplayWidth(dpy, 0);
    height = XDisplayHeight(dpy, 0);

    XResizeWindow (dpy, stage_win, width, height);
    {
        long event_mask;
        unsigned char mask_bits[XIMaskLen (XI_LASTEVENT)] = { 0 };
        XIEventMask mask = { XIAllMasterDevices, sizeof (mask_bits), mask_bits };
        XWindowAttributes attr;

        meta_core_add_old_event_mask (dpy, stage_win, &mask);

        XISetMask (mask.mask, XI_KeyPress);
        XISetMask (mask.mask, XI_KeyRelease);
        XISetMask (mask.mask, XI_ButtonPress);
        XISetMask (mask.mask, XI_ButtonRelease);
        XISetMask (mask.mask, XI_Enter);
        XISetMask (mask.mask, XI_Leave);
        XISetMask (mask.mask, XI_FocusIn);
        XISetMask (mask.mask, XI_FocusOut);
        XISetMask (mask.mask, XI_Motion);
        XISelectEvents (dpy, stage_win, &mask, 1);

        event_mask = ExposureMask | PropertyChangeMask | StructureNotifyMask;
        if (XGetWindowAttributes (dpy, stage_win, &attr))
        event_mask |= attr.your_event_mask;

        XSelectInput (dpy, stage_win, event_mask);
    }


    clutter_color_from_string(&color, "DarkSlateGrey");
    clutter_stage_set_color (CLUTTER_STAGE (stage), &color);
    
    for (index = 0; index < RECTS; index++)
    {
        rects[index] = clutter_rectangle_new_with_color(CLUTTER_COLOR_White);
        clutter_actor_set_size(rects[index], 10, 10);
        clutter_actor_set_position(rects[index], 800, 400);
        clutter_container_add_actor (CLUTTER_CONTAINER (stage), rects[index]);
        move_rect(rects[index]);
    }

    g_signal_connect (stage, "button-press-event", G_CALLBACK (on_stage_button_press), NULL);

    XReparentWindow (dpy, stage_win, overlay, 0, 0);
    //XSelectInput (dpy, stage_win, ExposureMask);
    //allow_input_passthrough (stage_win);
    
    XFixesSetWindowShapeRegion (dpy, overlay, ShapeBounding, 0, 0, None);

    XFixesSetWindowShapeRegion (dpy, stage_win, ShapeInput, 0, 0, pending_input_region);
    //meta_display_add_ignored_crossing_serial (display, XNextRequest (xdpy));
    XFixesSetWindowShapeRegion (dpy, overlay, ShapeInput, 0, 0, pending_input_region);
    if (pending_input_region != None)
    {
      XFixesDestroyRegion (dpy, pending_input_region);
      pending_input_region = None;
    }


    clutter_actor_show (stage);
    XMapWindow (dpy, overlay);

    XCompositeRedirectSubwindows (dpy, root, CompositeRedirectAutomatic);
    XSync (dpy, FALSE);


    //prep_input ();

    g_main_loop_run (g_main_loop_new (NULL, FALSE));
    return 0;
}

