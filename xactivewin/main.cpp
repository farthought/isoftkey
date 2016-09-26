/* Copyright (C) 2014 - 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <limits.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/extensions/XTest.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

//#include <X11/extensions/XTest.h>
#define DEBUG

using namespace std;

static const std::string grabName = "OVDNativeClient.jar";
//static const std::string grabName = "gedit";

static Display *display = NULL;
static Window root = -1;
static Atom atom_active_win = None;
static Atom atom_wm_pid = None;

static void cleanup()
{
    if (display) {
        XCloseDisplay(display);
        display = NULL;
    }
}

//static void send_accelerator()
//{
    //XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Control_L), True, 0);
    //XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_space), True, 0);

    //XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_Control_L), False, 100);
    //XTestFakeKeyEvent(display, XKeysymToKeycode(display, XK_space), False, 100);
//}

unsigned char* getWindowProperty(Window window, Atom atom, long *nitems,
                                 Atom *type, int *size)
{
    Atom actual_type;
    int actual_format;
    unsigned long _nitems;
    unsigned long bytes_after;
    unsigned char *prop;
    int status;

    status = XGetWindowProperty(display, window, atom, 0, (~0L),
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &_nitems, &bytes_after,
                                &prop);
    if (status == BadWindow)
        return NULL;
    if (status != Success)
        return NULL;

    if (nitems != NULL)
        *nitems = _nitems;

    if (type != NULL)
        *type = actual_type;

    if (size != NULL)
        *size = actual_format;

    return prop;
}

static void getWindowPid(Window wid, int* pid_ret)
{
    Atom type;
    int size;
    long nitems;
    unsigned char* data = getWindowProperty(wid, atom_wm_pid, &nitems, &type, &size);

    if (data == NULL)
        return;

    if (nitems > 0)
        *pid_ret = *((int*)data);

    if (data) {
        free(data);
        data = NULL;
    }
}

static std::string getPidName(int pid)
{
    std::ifstream file;
    char buf[PATH_MAX] = {'\0'};
    std::stringstream stream;

    snprintf(buf, sizeof(buf) - 1, "/proc/%d/cmdline", pid);
    file.open(buf);
    stream << file.rdbuf();
    file.close();

    return stream.str();
}

static void getActiveWindow(Window *window_ret)
{
    Atom type;
    int size;
    long nitems;
    unsigned char *data;

    data = getWindowProperty(root, atom_active_win, &nitems, &type, &size);
    if (nitems > 0)
        *window_ret = *((Window*)data);

    if (data) {
        free(data);
        data = NULL;
    }
}

static bool isGrabPid(Window active_wid)
{
#if 1
    int pid;
    std::string pname;

    getWindowPid(active_wid, &pid);
    pname = getPidName(pid);
#ifdef DEBUG
    std::cout << "DEBUG: " << __PRETTY_FUNCTION__ << " " << active_wid << " "
              << pid << " " << pname << std::endl;
#endif
    if (pname.find(grabName) != std::string::npos){
	return true;
	}
#endif
	return false;
}

static unsigned int current_mod = 0;

static void do_ungrab()
{
    XUngrabKey(display, XKeysymToKeycode(display, XK_space),
            current_mod, root);
}

static void do_grab()
{
    XGrabKey(display, XKeysymToKeycode(display, XK_space),
            current_mod, root, False, GrabModeAsync, GrabModeAsync);
}

static void do_sendkey(Window win, KeySym key, unsigned int mod)
{
    XEvent ev;
    memset(&ev, 0, sizeof ev);

    ev.type = KeyPress;
    ev.xkey.x = 1;
    ev.xkey.y = 1;
    ev.xkey.x_root = 1;
    ev.xkey.y_root = 1;
    ev.xkey.subwindow = None;
    ev.xkey.display = display;
    ev.xkey.root = root;
    ev.xkey.time = CurrentTime;
    ev.xkey.keycode = XKeysymToKeycode(display, key);
    ev.xkey.state = mod;
    ev.xkey.send_event = True;
    ev.xkey.same_screen = True;
    ev.xkey.type = KeyPress;

    Window focus = None;
    int revert;
    XGetInputFocus(display, &focus, &revert);
    ev.xkey.window = focus;
    XSendEvent(display, focus, True, 0, (XEvent*)&ev);

    ev.type = KeyRelease;
    ev.xkey.type = KeyRelease;
    XSendEvent(display, focus, True, 0, (XEvent*)&ev);
    XSync(display, False);
}

static void check_and_set_mod()
{
    //Mod2: Numlock, Mod5: Scrolllock, LockMask: Capslock
    unsigned int mod = ControlMask;
    XKeyboardState ks;
    XGetKeyboardControl(display, &ks);
    if (ks.led_mask & 2) {
        mod |= Mod2Mask;
        cout << __func__ << ": num lock on" << endl;
    } else {
        mod &= ~Mod2Mask;
        cout << __func__ << ": num lock off" << endl;
    }

    if (ks.led_mask & 1) {
        mod |= LockMask;
        cout << __func__ << ": caps lock on" << endl;
    } else {
        mod &= ~LockMask;
        cout << __func__ << ": caps lock off" << endl;
    }

    if (ks.led_mask & 4) {
        mod |= Mod5Mask;
        cout << __func__ << ": scroll lock on" << endl;
    } else {
        mod &= ~Mod5Mask;
        cout << __func__ << ": scroll lock off" << endl;
    }

    if (mod != current_mod) {
        do_ungrab();
        cout << __func__ << "mod changed, regrab" << endl;
        current_mod = mod;
        do_grab();
    }
}

static void switch_im_config(int on)
{
	static bool flag = false;
    if (!on) {
	flag =true;
        system("fcitx-remote -c");
        system("sed -ri 's,TriggerKey=.*,TriggerKey=,' $HOME/.config/fcitx/config");
        system("sed -in 's,sogoupinyin:True,sogoupinyin:False,' $HOME/.config/fcitx/profile");
        system("sed -in 's,wubi:True,wubi:False,' $HOME/.config/fcitx/profile");
        system("fcitx-remote -r");
    } else {
	if (flag == true) {
        system("sed -ri 's,TriggerKey=.*,TriggerKey=CTRL_SPACE CTRL_SPACE,' $HOME/.config/fcitx/config");
        system("sed -in 's,sogoupinyin:False,sogoupinyin:True,' $HOME/.config/fcitx/profile");
        system("sed -in 's,wubi:False,wubi:True,' $HOME/.config/fcitx/profile");
        system("fcitx-remote -r");
	flag = false;
	}
    }
}

static void doEvent()
{
    XEvent ev;
    XKeyEvent kev;
    XPropertyEvent pe;
    Window active_wid = 0;


    while (1) {
        XNextEvent(display, &ev);

        switch (ev.type) {
        case KeyRelease:
            std::cout << "KeyReleases " << endl;
            getActiveWindow(&active_wid);
            if (active_wid && isGrabPid(active_wid)) {
                check_and_set_mod();
                do_sendkey(active_wid, XK_F7, ControlMask|ShiftMask);
                // do_sendkey(active_wid, XK_F, Mod1Mask);
            }
            break;

        case PropertyNotify:
		std::cout << "PropertyNotify" << endl;
            pe = ev.xproperty;
            if (atom_active_win == pe.atom && pe.state == PropertyNewValue) {
                getActiveWindow(&active_wid);
                if (active_wid == 0)
                    break;

                if (isGrabPid(active_wid)) {

                    cout << "detect target window" << endl;
                    Window focus = None;
                    int revert;
                    static int cnt = 0;
                    XGetInputFocus(display, &focus, &revert);
                    XSetInputFocus (display, focus, RevertToParent, CurrentTime);

                    //XGrabKey(display, XKeysymToKeycode(display, XK_Scroll_Lock),
                    //        AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
                    //XGrabKey(display, XKeysymToKeycode(display, XK_Num_Lock),
                    //        AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
                    //XGrabKey(display, XKeysymToKeycode(display, XK_Caps_Lock),
                    //        AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
                    // check_and_set_mod();
                    // do_grab();
                    // do_sendkey(active_wid, XK_space, ControlMask);
                    switch_im_config(False);
                } else {
                    switch_im_config(True);
                    // do_ungrab();
                }
            }
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    display = XOpenDisplay(NULL);
    if (!display) {
        printf("ERROR: fail to open display\n");
        return 1;
    }
	std::cout << "in isoftkey main\n";

    atom_active_win = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    atom_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    root = DefaultRootWindow(display);
    XSelectInput(display, root, PropertyChangeMask | KeyReleaseMask);

    Window active_wid = 0;
    getActiveWindow(&active_wid);
    if (active_wid && isGrabPid(active_wid)) {
       // XGrabKey(display, XKeysymToKeycode(display, XK_Num_Lock),
       //         AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
       // XGrabKey(display, XKeysymToKeycode(display, XK_Scroll_Lock),
       //         AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
       // XGrabKey(display, XKeysymToKeycode(display, XK_Caps_Lock),
       //         AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
       // check_and_set_mod();
        switch_im_config(False);
    }

    //do_sendkey(active_wid, XK_space, ControlMask);
    //send_accelerator();
    doEvent();
    cleanup();

    return 0;
}
