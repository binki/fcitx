/***************************************************************************
 *   Copyright (C) 2002~2005 by Yuking                                     *
 *   yuking_net@sohu.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <cairo.h>
#include <limits.h>
#include <libintl.h>
#include <errno.h>

#include "core/fcitx.h"
#include "core/ui.h"
#include "core/module.h"
#include "module/x11/x11stuff.h"

#include "classicui.h"
#include "fcitx-config/xdg.h"
#include "fcitx-config/cutils.h"



struct FcitxSkin;

typedef struct FcitxClassicUIStatus {
    MouseE mouse;
    cairo_surface_t* active;
    cairo_surface_t* inactive;
} FcitxClassicUIStatus;

FcitxClassicUI classicui;

#define GetPrivateStatus(status) ((FcitxClassicUIStatus*)(status)->priv)

static boolean ClassicUIInit();
static void ClassicUICloseInputWindow();
static void ClassicUIShowInputWindow();
static void ClassicUIMoveInputWindow();
static void ClassicUIUpdateStatus(FcitxUIStatus* status);
static void ClassicUIRegisterStatus(FcitxUIStatus* status);
static void ClassicUIOnInputFocus();
static ConfigFileDesc* GetClassicUIDesc();

static void LoadClassicUIConfig();
static void SaveClassicUIConfig();

FCITX_EXPORT_API
FcitxUI ui = {
    ClassicUIInit,
    ClassicUICloseInputWindow,
    ClassicUIShowInputWindow,
    ClassicUIMoveInputWindow,
    ClassicUIUpdateStatus,
    ClassicUIRegisterStatus,
    ClassicUIOnInputFocus
};

boolean ClassicUIInit()
{
    FcitxModuleFunctionArg arg;
    classicui.dpy = InvokeFunction(FCITX_X11, GETDISPLAY, arg);
    
    XLockDisplay(classicui.dpy);
    
    classicui.iScreen = DefaultScreen(classicui.dpy);
    
    classicui.protocolAtom = XInternAtom (classicui.dpy, "WM_PROTOCOLS", False);
    classicui.killAtom = XInternAtom (classicui.dpy, "WM_DELETE_WINDOW", False);
    classicui.windowTypeAtom = XInternAtom (classicui.dpy, "_NET_WM_WINDOW_TYPE", False);
    classicui.typeMenuAtom = XInternAtom (classicui.dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    classicui.typeDialogAtom = XInternAtom (classicui.dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    classicui.typeDockAtom = XInternAtom (classicui.dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    classicui.pidAtom = XInternAtom(classicui.dpy, "_NET_WM_PID", False);
    
    InitComposite();
    LoadClassicUIConfig();
    LoadSkinConfig(&classicui.skin, &classicui.skinType);
    classicui.skin.skinType = &classicui.skinType;

    classicui.inputWindow = CreateInputWindow(classicui.dpy, classicui.iScreen, &classicui.skin, classicui.font);
    
    XUnlockDisplay(classicui.dpy);
    return true;
}

void SetWindowProperty(FcitxClassicUI* classicui, Window window, FcitxXWindowType type, char *windowTitle)
{
    Atom* wintype = NULL;
    switch(type)
    {
        case FCITX_WINDOW_DIALOG:
            wintype = &classicui->typeDialogAtom;
            break;
        case FCITX_WINDOW_DOCK:
            wintype = &classicui->typeDockAtom;
            break;
        case FCITX_WINDOW_MENU:
            wintype = &classicui->typeMenuAtom;
            break;
        default:
            wintype = NULL;
            break;
    }
    if (wintype)
        XChangeProperty (classicui->dpy, window, classicui->windowTypeAtom, XA_ATOM, 32, PropModeReplace, (void *) wintype, 1);

    pid_t pid = getpid();
    XChangeProperty(classicui->dpy, window, classicui->pidAtom, XA_CARDINAL, 32,
            PropModeReplace, (unsigned char *)&pid, 1);
    
    if (windowTitle)
    {
        XTextProperty   tp;
        Xutf8TextListToTextProperty(classicui->dpy, &windowTitle, 1, XUTF8StringStyle, &tp);
        XSetWMName (classicui->dpy, window, &tp);
        XFree(tp.value);
    }
}

static void ClassicUICloseInputWindow()
{
    CloseInputWindowInternal(classicui.inputWindow);
}

static void ClassicUIShowInputWindow()
{
    ShowInputWindowInternal(classicui.inputWindow);
}

static void ClassicUIMoveInputWindow()
{
    MoveInputWindowInternal(classicui.inputWindow);
}

static void ClassicUIUpdateStatus(FcitxUIStatus* status)
{
}

static void ClassicUIRegisterStatus(FcitxUIStatus* status)
{
    FcitxSkin* sc = &classicui.skin;
    status->priv = malloc(sizeof(FcitxClassicUIStatus));
    FcitxClassicUIStatus* privstat = GetPrivateStatus(status);
    char activename[PATH_MAX], inactivename[PATH_MAX];
    sprintf(activename, "%s_active.png", status->name);
    sprintf(inactivename, "%s_inactive.png", status->name);
    
    LoadImage(activename , *sc->skinType, &privstat->active, False);
    LoadImage(inactivename, *sc->skinType, &privstat->inactive, False);
}

static void ClassicUIOnInputFocus()
{
}

int
StringWidth(const char *str, const char *font, int fontSize)
{
    if (!str || str[0] == 0)
        return 0;
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, 10, 10);
    cairo_t        *c = cairo_create(surface);
    SetFontContext(c, font, fontSize);

    int             width = StringWidthWithContext(c, str);
    ResetFontContext();

    cairo_destroy(c);
    cairo_surface_destroy(surface);

    return width;
}

#ifdef _ENABLE_PANGO
int
StringWidthWithContextReal(cairo_t * c, PangoFontDescription* fontDesc, const char *str)
{
    if (!str || str[0] == 0)
        return 0;
    if (!utf8_check_string(str))
        return 0;

    int             width;
    PangoLayout *layout = pango_cairo_create_layout (c);
    pango_layout_set_text (layout, str, -1);
    pango_layout_set_font_description (layout, fontDesc);
    pango_layout_get_pixel_size(layout, &width, NULL);
    g_object_unref (layout);

    return width;
}
#else

int
StringWidthWithContextReal(cairo_t * c, const char *str)
{
    if (!str || str[0] == 0)
        return 0;
    if (!utf8_check_string(str))
        return 0;
    cairo_text_extents_t extents;
    cairo_text_extents(c, str, &extents);
    int             width = extents.x_advance;
    return width;
}
#endif

int
FontHeight(const char *font, int fontSize)
{
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, 10, 10);
    cairo_t        *c = cairo_create(surface);

    SetFontContext(c, font, fontSize);
    int             height = FontHeightWithContext(c);
    ResetFontContext();

    cairo_destroy(c);
    cairo_surface_destroy(surface);
    return height;
}

#ifdef _ENABLE_PANGO
int
FontHeightWithContextReal(cairo_t* c, PangoFontDescription* fontDesc)
{
    int height;

    if (pango_font_description_get_size_is_absolute(fontDesc)) /* it must be this case */
    {
        height = pango_font_description_get_size(fontDesc);
        height /= PANGO_SCALE;
    }
    else
        height = 0;

    return height;
}
#else

int
FontHeightWithContextReal(cairo_t * c)
{
    cairo_matrix_t matrix;
    cairo_get_font_matrix (c, &matrix);

    int             height = matrix.xx;
    return height;
}
#endif

/*
 * 以指定的颜色在窗口的指定位置输出字串
 */
void
OutputString(cairo_t * c, const char *str, const char *font, int fontSize, int x,
             int y, ConfigColor * color)
{
    if (!str || str[0] == 0)
        return;

    cairo_save(c);

    cairo_set_source_rgb(c, color->r, color->g, color->b);
    SetFontContext(c, font, fontSize);
    OutputStringWithContext(c, str, x, y);
    ResetFontContext();

    cairo_restore(c);
}

#ifdef _ENABLE_PANGO
void
OutputStringWithContextReal(cairo_t * c, PangoFontDescription* desc, const char *str, int x, int y)
{
    if (!str || str[0] == 0)
        return;
    if (!utf8_check_string(str))
        return;
    cairo_save(c);

    PangoLayout *layout;

    layout = pango_cairo_create_layout (c);
    pango_layout_set_text (layout, str, -1);
    pango_layout_set_font_description (layout, desc);
    cairo_move_to(c, x, y);
    pango_cairo_show_layout (c, layout);

    cairo_restore(c);
    g_object_unref (layout);
}
#else

void
OutputStringWithContextReal(cairo_t * c, const char *str, int x, int y)
{
    if (!str || str[0] == 0)
        return;
    if (!utf8_check_string(str))
        return;
    cairo_save(c);
    int             height = FontHeightWithContextReal(c);
    cairo_move_to(c, x, y + height);
    cairo_show_text(c, str);
    cairo_restore(c);
}
#endif

Bool
IsWindowVisible(Display* dpy, Window window)
{
    XWindowAttributes attrs;

    XGetWindowAttributes(dpy, window, &attrs);

    if (attrs.map_state == IsUnmapped)
        return False;

    return True;
}

void
InitWindowAttribute(Display* dpy, int iScreen, Visual ** vs, Colormap * cmap,
                    XSetWindowAttributes * attrib,
                    unsigned long *attribmask, int *depth)
{
    attrib->bit_gravity = NorthWestGravity;
    attrib->backing_store = WhenMapped;
    attrib->save_under = True;
    if (*vs) {
        *cmap =
            XCreateColormap(dpy, RootWindow(dpy, iScreen), *vs, AllocNone);

        attrib->override_redirect = True;       // False;
        attrib->background_pixel = 0;
        attrib->border_pixel = 0;
        attrib->colormap = *cmap;
        *attribmask =
            (CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWSaveUnder |
             CWColormap | CWBitGravity | CWBackingStore);
        *depth = 32;
    } else {
        *cmap = DefaultColormap(dpy, iScreen);
        *vs = DefaultVisual(dpy, iScreen);
        attrib->override_redirect = True;       // False;
        attrib->background_pixel = 0;
        attrib->border_pixel = 0;
        *attribmask = (CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWSaveUnder
                | CWBitGravity | CWBackingStore);
        *depth = DefaultDepth(dpy, iScreen);
    }
}

void ActivateWindow(Display *dpy, int iScreen, Window window)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));

    Atom _NET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);

    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = _NET_ACTIVE_WINDOW;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;
    ev.xclient.data.l[1] = CurrentTime;
    ev.xclient.data.l[2] = 0;

    XSendEvent(dpy, RootWindow(dpy, iScreen), False, SubstructureNotifyMask, &ev);
    XSync(dpy, False);
}

void GetScreenSize(Display* dpy, int iScreen, int *width, int *height)
{
    XWindowAttributes attrs;
    if (XGetWindowAttributes(classicui.dpy, RootWindow(dpy, iScreen), &attrs) < 0) {
        printf("ERROR\n");
    }
    if (width != NULL)
        (*width) = attrs.width;
    if (height != NULL)
        (*height) = attrs.height;
}

void InitComposite()
{
    classicui.compManagerAtom = XInternAtom (classicui.dpy, "_NET_WM_CM_S0", False);

    classicui.compManager = XGetSelectionOwner(classicui.dpy, classicui.compManagerAtom);

    if (classicui.compManager)
    {
        XSetWindowAttributes attrs;
        attrs.event_mask = StructureNotifyMask;
        XChangeWindowAttributes (classicui.dpy, classicui.compManager, CWEventMask, &attrs);
    }
}


ConfigFileDesc* GetClassicUIDesc()
{    
    static ConfigFileDesc * classicUIDesc = NULL;
    if (!classicUIDesc)
    {
        FILE *tmpfp;
        tmpfp = GetXDGFileData("fcitx-classic-ui.desc", "r", NULL);
        classicUIDesc = ParseConfigFileDescFp(tmpfp);
        fclose(tmpfp);
    }

    return classicUIDesc;
}

void LoadClassicUIConfig()
{
    FILE *fp;
    char *file;
    fp = GetXDGFileUser( "fcitx-classic-ui.config", "rt", &file);
    FcitxLog(INFO, _("Load Config File %s"), file);
    free(file);
    if (!fp) {
        if (errno == ENOENT)
        {
            SaveClassicUIConfig();
            LoadClassicUIConfig();
        }
        return;
    }
    
    ConfigFileDesc* configDesc = GetClassicUIDesc();
    ConfigFile *cfile = ParseConfigFileFp(fp, configDesc);
    
    FcitxClassicUIConfigBind(&classicui, cfile, configDesc);
    ConfigBindSync((GenericConfig*)&classicui);

    fclose(fp);

}

void SaveClassicUIConfig()
{
    ConfigFileDesc* configDesc = GetClassicUIDesc();
    char *file;
    FILE *fp = GetXDGFileUser("fcitx-classic-ui.config", "wt", &file);
    FcitxLog(INFO, "Save Config to %s", file);
    SaveConfigFileFp(fp, classicui.gconfig.configFile, configDesc);
    free(file);
    fclose(fp);
}

#ifdef _ENABLE_PANGO
PangoFontDescription* GetPangoFontDescription(const char* font, int size)
{
    PangoFontDescription* desc;
    desc = pango_font_description_new ();
    pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
    pango_font_description_set_family(desc, font);
    return desc;
}
#endif

Visual * FindARGBVisual (Display *dpy, int scr)
{
    XVisualInfo *xvi;
    XVisualInfo temp;
    int         nvi;
    int         i;
    XRenderPictFormat   *format;
    Visual      *visual;

    if (classicui.compManager == None)
        return NULL;

    temp.screen = scr;
    temp.depth = 32;
    temp.class = TrueColor;
    xvi = XGetVisualInfo (dpy,  VisualScreenMask |VisualDepthMask |VisualClassMask,&temp,&nvi);
    if (!xvi)
        return 0;
    visual = 0;
    for (i = 0; i < nvi; i++)
    {
        format = XRenderFindVisualFormat (dpy, xvi[i].visual);
        if (format->type == PictTypeDirect && format->direct.alphaMask)
        {
            visual = xvi[i].visual;
            break;
        }
    }

    XFree (xvi);
    return visual;
}

/*
 * 判断鼠标点击处是否处于指定的区域内
 */
boolean
IsInBox(int x0, int y0, int x1, int y1, int x2, int y2)
{
    if (x0 >= x1 && x0 <= x2 && y0 >= y1 && y0 <= y2)
        return true;

    return false;
}

Bool
MouseClick(int *x, int *y, Display* dpy, Window window)
{
    XEvent          evtGrabbed;
    Bool            bMoved = False;

    // To motion the window
    while (1) {
        XMaskEvent(dpy,
                   PointerMotionMask | ButtonReleaseMask | ButtonPressMask,
                   &evtGrabbed);
        if (ButtonRelease == evtGrabbed.xany.type) {
            if (Button1 == evtGrabbed.xbutton.button)
                break;
        } else if (MotionNotify == evtGrabbed.xany.type) {
            static Time     LastTime;

            if (evtGrabbed.xmotion.time - LastTime < 20)
                continue;

            XMoveWindow(dpy, window, evtGrabbed.xmotion.x_root - *x,
                        evtGrabbed.xmotion.y_root - *y);
            XRaiseWindow(dpy, window);

            bMoved = True;
            LastTime = evtGrabbed.xmotion.time;
        }
    }

    *x = evtGrabbed.xmotion.x_root - *x;
    *y = evtGrabbed.xmotion.y_root - *y;

    return bMoved;
}


/*
*把鼠标状态初始化为某一种状态.
*/
Bool SetMouseStatus()
{
    /* TODO 
    Bool changed = False;
    int i = 0;
    for (i = 0 ;i < 8; i ++)
    {
        MouseE obj;
        if (msE[i] == e)
            obj = s;
        else
            obj = m;
        
        if (obj != *msE[i])
            changed = True;
        
        *msE[i] = obj;
    }
    
    return changed;*/
    return True;
}