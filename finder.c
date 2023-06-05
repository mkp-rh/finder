// C
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// POSIX
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>

// X11
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

static char mutex_path[] = "/dev/shm/finder";
static int mutex_fd = -1;
static int may_run = 0;
static int show_lines = 0;
static pthread_t thread_id = 0;
static Display *d;
static int daemonize = 0;


int mutex_lock() {
    int ret;
    mutex_fd = open(mutex_path, O_CREAT | O_RDWR, 0600);

    if (mutex_fd == -1) {
        printf("failed to open %s, error %i\n", mutex_path, errno);
        return 1;
    }

    ret = flock(mutex_fd, LOCK_EX | LOCK_NB);
    if (ret < 0) {
        printf("failed to lock %s, error %i\n", mutex_path, errno);

        close(mutex_fd);
        mutex_fd = -1;
        return 1;
    }

    return 0;
}

void mutex_reportpid() {
    char s[21];
    long pid = (long)getpid();
    int n = snprintf(s, sizeof(s), "%li", pid);
    if (n < 0) {
        return;
    }
    write(mutex_fd, s, n);
    fsync(mutex_fd);
}

void mutex_unlock() {
    int ret;

    if (mutex_fd == -1) {
        return;
    }

    ret = flock(mutex_fd, LOCK_UN);
    if (ret < 0) {
        if (!daemonize) {
            printf("unlock failed\n");
        }
        return;
    }

    ret = close(mutex_fd);
    if (ret == -1) {
        if (!daemonize) {
            printf("close failed\n");
        }
        return;
    }

    unlink(mutex_path);
}

void *line_thread(void* x) {
    Window pointer_window;
    while ( may_run ) {
        if ( !show_lines ) { 
            usleep(10000); // 10ms
            continue;
        }
        for(int scr_no = 0; scr_no < XScreenCount(d); scr_no++) {
            Window root = XRootWindow(d, scr_no);
            Screen *screen = XScreenOfDisplay(d, scr_no);

            // Get colour attributes
            XSetWindowAttributes attrs;
            attrs.override_redirect = True;
            attrs.background_pixel = 0;
            attrs.border_pixel = 0;
            XVisualInfo vinfo;
            if (!XMatchVisualInfo(d, scr_no, 32, TrueColor, &vinfo)) {
                continue;
            }
            attrs.colormap = XCreateColormap(d, root, vinfo.visual, AllocNone);

            // Screen size
            int width = WidthOfScreen(screen);
            int height = HeightOfScreen(screen);

            // Find cursor
            int root_x, root_y, win_x, win_y;
            unsigned int mask_return;
            if (!XQueryPointer(d, root, &pointer_window, &pointer_window,
                        &root_x, &root_y, &win_x, &win_y, &mask_return)) {
                continue;
            }

            // Overlay window
            Window overlay = XCreateWindow(
                d, root,
                0, 0, width, height, 0,
                vinfo.depth, InputOutput, 
                vinfo.visual,
                CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel, &attrs
            );

            XMapWindow(d, overlay);

            // Show lines
            cairo_surface_t* surf = cairo_xlib_surface_create(d, overlay,
                                          vinfo.visual,
                                          width, height);
            cairo_t* cr = cairo_create(surf);

            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
            cairo_set_line_width(cr, 1);

            for (int a=0; a<=8; a++) {
                for (int b=0; b<=8; b++) {
                    if ( a == 0 || a == 8 || b == 0 || b == 8) {
                        cairo_move_to(cr, (width>>3)*a, (height>>3)*b);
                        cairo_line_to(cr, win_x, win_y);
                    }
                }
            }

            cairo_stroke(cr);

            XFlush(d);

            while ( show_lines ) {
                usleep(1000);
            }

            cairo_destroy(cr);
            cairo_surface_destroy(surf);

            // Clean up windows
            XUnmapWindow(d, overlay);
            XDestroyWindow(d, overlay);
            XFlush(d);
        }
    }
    return NULL;
}

void close_display(int sig) {
    void * res;
    if (!daemonize) {
        printf("shutting down\n");
    }
    may_run = 0;
    if( thread_id ) {
        pthread_join(thread_id, &res);
    }
    XCloseDisplay(d);
    mutex_unlock();
    exit(0);
}

void initialize(int daemonize) {
    // Daemonize
    if (daemonize) {
        daemon(0, 1);
    }

    mutex_reportpid();

    // Open main display
    d = XOpenDisplay(NULL);

    // Listen for common close signals
    signal(SIGINT, close_display);
    signal(SIGABRT, close_display);
    signal(SIGTERM, close_display);
    signal(SIGBUS, close_display);
    signal(SIGSEGV, close_display);
}

void launch_thread() {
    int s;
    pthread_attr_t attr;

    may_run = 1;
    s = pthread_attr_init(&attr);
    if (s) {
        close_display(0);
    }
    s = pthread_create(&thread_id, &attr,
                       &line_thread, NULL);
    if (s) {
        close_display(0);
    }
}

void event_loop() {
    char key_map[32];
    int is_pressed = 0;

    while(1) {
        XQueryKeymap(d, key_map);
        KeyCode keycode = XKeysymToKeycode(d, XK_Pause);
        is_pressed = !!(key_map[keycode >> 3] & (1 << (keycode % 8)));

        if ( show_lines && ! is_pressed ) {
            show_lines = 0;
        } else if ( ! show_lines && is_pressed ) {
            show_lines = 1;
        }

        usleep(100000); // 100ms
    }
}

int main(int c, char ** v) {
    if ( c > 1 && strstr(v[1], "-d") != NULL) {
        daemonize = 1;
    }

    if (mutex_lock() == 1) {
        printf("failed to obtain global mutex %s\n", mutex_path);
        return 1;
    }

    initialize( daemonize );

    launch_thread();

    event_loop();

    return 0;
}
