/* mini-clutter-wm.c
 * Copyright (C) 2008 Andy Wingo <wingo at pobox dot com>
 *
 * mini-clutter-wm.c: minimal window manager based on clutter
 *
 * I release this code into the public domain.
 *
 * Link: http://wingolog.org/archives/2008/07/26/so-you-want-to-build-a-compositor
 */

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <clutter/glx/clutter-glx.h>
#include <stdlib.h>

#define RECTS 100

Display *dpy;
Window root, overlay, stage_win, input;
ClutterActor *stage;
int stage_w, stage_h;
GType texture_pixmap_type;

GPollFD event_poll_fd;

ClutterActor *rects[RECTS];

static ClutterX11FilterReturn event_filter (XEvent *ev, ClutterEvent *cev,
                                            gpointer unused);

static gboolean
event_prepare (GSource *source, gint *timeout)
{
    gboolean retval;

    clutter_threads_enter ();

    *timeout = -1;
    retval = clutter_events_pending () || XPending (dpy);

    clutter_threads_leave ();

    return retval;
}

static gboolean
event_check (GSource *source)
{
    gboolean retval;

    clutter_threads_enter ();

    if (event_poll_fd.revents & G_IO_IN)
        retval = clutter_events_pending () || XPending (dpy);
    else
        retval = FALSE;

    clutter_threads_leave ();

    return retval;
}

static gboolean
event_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
    ClutterEvent *event;
    XEvent xevent;

    clutter_threads_enter ();

    while (!clutter_events_pending () && XPending (dpy)) {
        XNextEvent (dpy, &xevent);
        
        /* here the trickiness */
        if (xevent.xany.window == input) {
            xevent.xany.window = stage_win;
        }

        clutter_x11_handle_event (&xevent);
    }
    
    if ((event = clutter_event_get ())) {
        clutter_do_event (event);
        clutter_event_free (event);
    }

    clutter_threads_leave ();

    return TRUE;
}

static GSourceFuncs event_funcs = {
    event_prepare,
    event_check,
    event_dispatch,
    NULL
};

void
prep_clutter (int *argc, char ***argv)
{
    //clutter_x11_disable_event_retrieval ();
    int r = clutter_init (argc, argv);
    //clutter_x11_add_filter (event_filter, NULL);
    
    if (getenv("NO_TFP"))
        texture_pixmap_type = CLUTTER_X11_TYPE_TEXTURE_PIXMAP;
    else
        texture_pixmap_type = CLUTTER_GLX_TYPE_TEXTURE_PIXMAP;
}

void
prep_root (void)
{
    dpy = clutter_x11_get_default_display ();
    root = DefaultRootWindow (dpy);

    //XCompositeRedirectSubwindows (dpy, root, CompositeRedirectAutomatic);
    //XSelectInput (dpy, root, SubstructureNotifyMask);
}

void
allow_input_passthrough (Window w)
{
    XserverRegion region = XFixesCreateRegion (dpy, NULL, 0);
    
    XFixesSetWindowShapeRegion (dpy, w, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion (dpy, w, ShapeInput, 0, 0, region);

    XFixesDestroyRegion (dpy, region);
}
    
void
prep_overlay (void)
{
    overlay = XCompositeGetOverlayWindow (dpy, root);
    //allow_input_passthrough (overlay);
}

void move_rect(ClutterActor *rect) {
  clutter_actor_animate (rect,
          CLUTTER_EASE_IN_OUT_CUBIC,
          (rand() / (float)RAND_MAX) * 200 + 5000,
          "x", (float)(rand() % 800),
          "y", (float)(rand() % 300),
          "width", (float)(rand() % 300),
          "height", (float)(rand() % 300),
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

void
prep_stage (void)
{
    ClutterColor color;
    int index,
        width, 
        height;

    stage = clutter_stage_new();
    clutter_stage_set_fullscreen (CLUTTER_STAGE (stage), TRUE);
    
    clutter_actor_realize(stage);

    stage_win = clutter_x11_get_stage_window (CLUTTER_STAGE (stage));

    width  = XDisplayWidth(dpy, 0);
    height = XDisplayHeight(dpy, 0);

    XResizeWindow (dpy, stage_win, width, height);

    clutter_color_from_string(&color, "DarkSlateGrey");
    clutter_stage_set_color (CLUTTER_STAGE (stage), &color);
    
    for (index = 0; index < RECTS; index++)
    {
        rects[index] = clutter_rectangle_new_with_color(CLUTTER_COLOR_Black);
        clutter_actor_set_size(rects[index], 10, 10);
        clutter_actor_set_position(rects[index], 800, 400);
        clutter_container_add_actor (CLUTTER_CONTAINER (stage), rects[index]);
        move_rect(rects[index]);
    }

    g_signal_connect (stage, "button-press-event", G_CALLBACK (on_stage_button_press), NULL);

    XReparentWindow (dpy, stage_win, overlay, 0, 0);
    //XSelectInput (dpy, stage_win, ExposureMask);
    //allow_input_passthrough (stage_win);
    
    clutter_actor_show_all (stage);
}

void
attach_event_source (void)
{
    GSource *source;

    source = g_source_new (&event_funcs, sizeof (GSource));

    event_poll_fd.fd = ConnectionNumber (dpy);
    event_poll_fd.events = G_IO_IN;

    g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
    g_source_add_poll (source, &event_poll_fd);
    g_source_set_can_recurse (source, TRUE);
    g_source_attach (source, NULL);
}

void
prep_input (void)
{
    /*
    XWindowAttributes attr;

    XGetWindowAttributes (dpy, root, &attr);
    input = XCreateWindow (dpy, root,
                           0, 0,  // x, y
                           attr.width, attr.height,
                           0, 0, // border width, depth
                           InputOnly, DefaultVisual (dpy, 0), 0, NULL);
    XSelectInput (dpy, input,
                  StructureNotifyMask | FocusChangeMask | PointerMotionMask
                  | KeyPressMask | KeyReleaseMask | ButtonPressMask
                  | ButtonReleaseMask | PropertyChangeMask);
    XMapWindow (dpy, input);
    XSetInputFocus (dpy, input, RevertToPointerRoot, CurrentTime);

    attach_event_source ();*/
    XserverRegion region = XFixesCreateRegion (dpy, NULL, 0);
    Window        xstage = clutter_x11_get_stage_window(CLUTTER_STAGE(stage));

    XFixesSetWindowShapeRegion (dpy, xstage, ShapeInput, 0, 0, region);

    //do_set_stage_input_region (screen, region);
    if (region != None)
    {
      XFixesDestroyRegion (dpy, region);
      region = None;
    }
}


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
    g_print ("window created\n");
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

    {
        gint mapped, destroyed;
        g_object_get (tex, "mapped", &mapped, "destroyed", &destroyed, NULL);
        
        if (mapped)
            window_mapped_changed (tex, NULL, NULL);
        if (destroyed)
            window_destroyed (tex, NULL, NULL);
    }
}

static gboolean
handle_keypress (Window w, unsigned int state, unsigned int keycode)
{
    g_print ("key pressed: %u %u %u\n", w, state, keycode);
    return FALSE;
}

static ClutterX11FilterReturn
event_filter (XEvent *ev, ClutterEvent *cev, gpointer unused)
{
    switch (ev->type) {
    case CreateNotify:
        window_created (ev->xcreatewindow.window);
        return CLUTTER_X11_FILTER_REMOVE;

    case KeyPress:
        if (handle_keypress (ev->xkey.window, ev->xkey.state, ev->xkey.keycode))
            return CLUTTER_X11_FILTER_REMOVE;
        else
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

int main (int argc, char *argv[])
{
    prep_clutter (&argc, &argv);
    prep_root ();
    prep_overlay ();
    prep_stage ();
    //prep_input ();

    g_main_loop_run (g_main_loop_new (NULL, FALSE));

    return 0;
}

