#include "libx11cocoainput.h"
#include <locale.h>
#include <stdlib.h>
#include <string.h>

void (*javaDone)();
int *(*javaDraw)(int, int, int, short, int, char *, wchar_t *, int, int, int);
XICCallback calet, start, done, draw;
XICCallback s_start, s_done, s_draw;

// X11 error handler
int x11_error_code = 0;
int x11_error_handler(Display *display, XErrorEvent *error) {
    x11_error_code = error->error_code;
    char error_text[256];
    XGetErrorText(display, error->error_code, error_text, sizeof(error_text));
    CIError("X11 Error: %s (code: %d, request: %d, minor: %d)", 
            error_text, error->error_code, error->request_code, error->minor_code);
    return 0;
}

void setCallback(int *(*c_draw)(int, int, int, short, int, char *, wchar_t *, int, int, int), void (*c_done)()) {
    javaDraw = c_draw;
    javaDone = c_done;
}

int preeditCalet(XIC xic, XPointer clientData, XPointer data) {
    return 0;
}
int preeditStart(XIC xic, XPointer clientData, XPointer data) {
    CIDebug("Preedit start");
    
    // Try to set a default position at start
    XVaNestedList attr;
    XPoint place;
    
    // Get window geometry to position at bottom-left
    Window root;
    int x, y;
    unsigned int width, height, border, depth;
    Window focus_window;
    int revert_to;
    XGetInputFocus(XDisplayOfIM(XIMOfIC(xic)), &focus_window, &revert_to);
    
    if (XGetGeometry(XDisplayOfIM(XIMOfIC(xic)), focus_window, &root, &x, &y, &width, &height, &border, &depth)) {
        // Position near bottom-left with some padding
        place.x = 10;  // 10 pixels from left
        place.y = height - 50;  // 50 pixels from bottom
        CIDebug("Window geometry: %dx%d, setting initial spot to: %d,%d", width, height, place.x, place.y);
        
        attr = XVaCreateNestedList(0, XNSpotLocation, &place, NULL);
        XSetICValues(xic, XNPreeditAttributes, attr, NULL);
        XFree(attr);
    }
    
    return 0;
}
int preeditDone(XIC xic, XPointer clientData, XPointer data) {
    CIDebug("Preedit end");
    javaDone();
    return 0;
}
int preeditDraw(XIC xic, XPointer clientData, XPointer structptr) {
    CIDebug("Preedit draw start");
    XIMPreeditDrawCallbackStruct *structure = (XIMPreeditDrawCallbackStruct *)structptr;
    int *array;
    int secondary = 0;
    int length = 0;
    if (structure->text) {
        int i = 0;
        int secondary_determined = 0;
        for (i = 0; i != structure->text->length; i++) {
            if (!secondary_determined && structure->text->feedback[i] != XIMUnderline) {
                secondary = i;
                secondary_determined = 1;
            }
            if (secondary_determined && (structure->text->feedback[i] == 0 || structure->text->feedback[i] != XIMUnderline)) {
                length++;
            } else if (secondary_determined) {
                break;
            }
        }
    }

    CIDebug("Invoke Javaside");
    if (structure->text) {
        array = javaDraw(
            structure->caret,
            structure->chg_first,
            structure->chg_length,
            structure->text->length,
            structure->text->encoding_is_wchar,
            structure->text->encoding_is_wchar ? "" : structure->text->string.multi_byte,
            structure->text->encoding_is_wchar ? structure->text->string.wide_char : L"",
            0,
            secondary,
            secondary + length
        );
    }
    else {
        array = javaDraw(
            structure->caret,
            structure->chg_first,
            structure->chg_length,
            0,
            0,
            "",
            L"",
            0,
            0,
            0
        );
    }

    // TODO なんとかしてon-the-spotの候補ウィンドウをちゃんとした位置にしたいね（願望）
    XVaNestedList attr;
    XPoint place;
    place.x = array[0];
    place.y = array[1];
    CIDebug("JavaDraw returned position: x=%d, y=%d", place.x, place.y);
    
    // If Java returns the placeholder 600,600, use a better default
    if (place.x == 600 && place.y == 600) {
        CIDebug("Detected placeholder coordinates (600,600), using bottom-left position");
        
        // Get window geometry to position at bottom-left
        Window root;
        int x, y;
        unsigned int width, height, border, depth;
        Window focus_window;
        int revert_to;
        XGetInputFocus(XDisplayOfIM(XIMOfIC(xic)), &focus_window, &revert_to);
        
        if (XGetGeometry(XDisplayOfIM(XIMOfIC(xic)), focus_window, &root, &x, &y, &width, &height, &border, &depth)) {
            // Position near bottom-left of window
            place.x = 10;  // 10 pixels from left
            place.y = height - 30;  // 30 pixels from bottom
            CIDebug("Using window-relative position: x=%d, y=%d (window: %dx%d)", place.x, place.y, width, height);
        }
    }
    
    attr = XVaCreateNestedList(0, XNSpotLocation, &place, NULL);
    XSetICValues(xic, XNPreeditAttributes, attr, NULL);
    XFree(attr);
    CIDebug("Preedit draw end");
    return 0;
}

int statusStart(XIC xic, XPointer clientData, XPointer data) {
    return 0;
}
int statusDone(XIC xic, XPointer clientData, XPointer data) {
    return 0;
}
int statusDraw(XIC xic, XPointer clientData, XPointer data /*,XIMStatusDrawCallbackStruct *structure*/) {
    return 0;
}

XVaNestedList preeditCallbacksList() {
    calet.client_data = NULL;
    start.client_data = NULL;
    done.client_data = NULL;
    draw.client_data = NULL;
    calet.callback = preeditCalet;
    start.callback = preeditStart;
    done.callback = preeditDone;
    draw.callback = preeditDraw;
    return XVaCreateNestedList(
        0,
        XNPreeditStartCallback,
        &start,
        XNPreeditCaretCallback,
        &calet,
        XNPreeditDoneCallback,
        &done,
        XNPreeditDrawCallback,
        &draw,
        NULL
    );
}

XVaNestedList statusCallbacksList() {
    s_start.client_data = NULL;
    s_done.client_data = NULL;
    s_draw.client_data = NULL;
    s_start.callback = statusStart;
    s_done.callback = statusDone;
    s_draw.callback = statusDraw;
    return XVaCreateNestedList(
        0,
        XNStatusStartCallback,
        &s_start,
        XNStatusDoneCallback,
        &s_done,
        XNStatusDrawCallback,
        &s_draw,
        NULL
    );
}

_GLFWwindowX11 *x11c; //_GLFWwindowX11のアドレス
XIM xim;              //_GLFWwindowX11が保持するXIM
Window xwindow;       //

XIC activeic;   // IMが有効なIC
XIC inactiveic; // IMが無効なIC

void initialize(
    long waddr,
    long xw,
    int *(*c_draw)(int, int, int, short, int, char *, wchar_t *, int, int, int),
    void (*c_done)(),
    LogFunction log,
    LogFunction error,
    LogFunction debug
) {
    initLogPointer(log, error, debug);
    CILog("CocoaInput X11 Clang Initializer start. library compiled at  %s %s", __DATE__, __TIME__);

    setCallback(c_draw, c_done);

    CIDebug("Window ptr:%p", (Window)xw);
    CIDebug("GLFWwindow ptr:%p", (void *)waddr);
    CIDebug("Searching _GLFWwindowx11 from GLFWwindow ptr...");
    int i;
    XIC ic = NULL;
    for (i = 0; i < 0x500; i++) {
        Window po = (*(((_GLFWwindowX11 *)(waddr + i)))).handle;
        if (po != xw) {
            continue;
        }

        x11c = (((_GLFWwindowX11 *)(waddr + i)));
        ic = (*(((_GLFWwindowX11 *)(waddr + i)))).ic;
        CIDebug("Found offset:%d ,_GLFWwindowX11(%p)=GLFWwindow(%p)+%d ", i, x11c, (void *)waddr, i);
        break;
    }
    CIDebug("XIC mem address:%p", x11c->ic);
    
    // Get the display from the IC first
    Display *display = XDisplayOfIM(XIMOfIC(ic));
    
    // Install error handler to catch X11 errors
    XErrorHandler old_handler = XSetErrorHandler(x11_error_handler);
    
    // Check current locale and modifiers
    char *current_locale = setlocale(LC_ALL, NULL);
    CILog("Current locale before: %s", current_locale ? current_locale : "NULL");
    
    // Check XMODIFIERS
    char *xmodifiers = XSetLocaleModifiers("");
    CILog("X locale modifiers: %s", xmodifiers ? xmodifiers : "NULL");
    
    // Try to open a proper XIM instead of using GLFW's limited one
    char *orig_locale = setlocale(LC_ALL, NULL);
    if (!XSupportsLocale()) {
        CIError("X does not support current locale");
    }
    setlocale(LC_ALL, "");  // Use environment locale
    
    // Try with explicit modifiers first
    char *env_xmod = getenv("XMODIFIERS");
    CILog("XMODIFIERS env: '%s'", env_xmod ? env_xmod : "not set");
    
    if (env_xmod && strlen(env_xmod) > 0) {
        XSetLocaleModifiers(env_xmod);
        xim = XOpenIM(display, NULL, NULL, NULL);
        if (xim != NULL) {
            CILog("Opened XIM with XMODIFIERS: %s", env_xmod);
        }
    }
    
    if (xim == NULL) {
        // Try common IM modifiers
        char *im_modifiers[] = {"@im=ibus", "@im=fcitx", "@im=fcitx5", "@im=scim", "@im=uim", NULL};
        for (int i = 0; im_modifiers[i] != NULL; i++) {
            CILog("Trying XSetLocaleModifiers: %s", im_modifiers[i]);
            XSetLocaleModifiers(im_modifiers[i]);
            xim = XOpenIM(display, NULL, NULL, NULL);
            if (xim != NULL) {
                CILog("Successfully opened XIM with modifiers: %s", im_modifiers[i]);
                
                // Query this XIM's supported styles
                XIMStyles *test_styles = NULL;
                XGetIMValues(xim, XNQueryInputStyle, &test_styles, NULL);
                if (test_styles) {
                    CILog("This XIM supports %d styles", (int)test_styles->count_styles);
                    for (int j = 0; j < test_styles->count_styles; j++) {
                        CILog("  Style %d: 0x%lx", j, (unsigned long)test_styles->supported_styles[j]);
                    }
                    XFree(test_styles);
                }
                break;
            }
        }
    }
    
    if (xim == NULL) {
        // Try with default modifiers
        CILog("Falling back to default modifiers");
        XSetLocaleModifiers("");
        xim = XOpenIM(display, NULL, NULL, NULL);
    }
    
    if (xim == NULL) {
        CIError("Failed to open XIM, falling back to GLFW's XIM");
        xim = XIMOfIC(ic);
    } else {
        CILog("Successfully opened our own XIM");
        
        // Get more info about the XIM
        XIMStyles *xim_styles = NULL;
        XGetIMValues(xim, 
                     XNQueryInputStyle, &xim_styles,
                     NULL);
        
        if (xim_styles) {
            CILog("Direct XIM query shows %d styles", (int)xim_styles->count_styles);
        }
    }
    
    // Restore locale
    if (orig_locale) {
        setlocale(LC_ALL, orig_locale);
    }
    
    CILog("XIM mem address:%p", xim);
    
    // Query supported input styles
    XIMStyles *im_supported_styles = NULL;
    char *failed_arg = XGetIMValues(xim, XNQueryInputStyle, &im_supported_styles, NULL);
    if (failed_arg != NULL) {
        CIError("Failed to query input styles: %s", failed_arg);
    } else if (im_supported_styles == NULL) {
        CIError("XGetIMValues succeeded but im_supported_styles is NULL");
    } else {
        CILog("XIM supports %d input styles:", (int)im_supported_styles->count_styles);
        for (int i = 0; i < im_supported_styles->count_styles; i++) {
            XIMStyle style = im_supported_styles->supported_styles[i];
            CILog("  Style %d: 0x%lx (Preedit: 0x%lx, Status: 0x%lx)",
                    i, (unsigned long)style, 
                    (unsigned long)(style & 0x00FF), 
                    (unsigned long)(style & 0xFF00));
        }
        XFree(im_supported_styles);
    }
    
    xwindow = xw;
    
    // Clear error code and sync to ensure we catch errors
    x11_error_code = 0;
    XSync(display, False);
    
    inactiveic = XCreateIC(
        xim,
        XNClientWindow,
        (Window)xwindow,
        XNFocusWindow,
        (Window)xwindow,
        XNInputStyle,
        XIMPreeditNone | XIMStatusNone,
        NULL
    );
    
    XSync(display, False);  // Force error handling
    
    if (inactiveic == NULL || x11_error_code != 0) {
        CIError("Failed to create inactiveic (error code: %d)", x11_error_code);
        XSetErrorHandler(old_handler);
        return;
    }
    CILog("Created inactiveic-> default");
    
    // Clear error code before creating activeic
    x11_error_code = 0;
    XSync(display, False);
    
    CILog("Attempting to create activeic with style 0x402 (XIMPreeditCallbacks | XIMStatusNothing)");
    
    // First try without attributes to see if that's the issue
    activeic = XCreateIC(
        xim,
        XNClientWindow,
        xwindow,
        XNFocusWindow,
        xwindow,
        XNInputStyle,
        XIMPreeditCallbacks | XIMStatusNothing,  // 0x402 - supported by IBus
        NULL
    );
    
    if (activeic != NULL) {
        CILog("Created IC without attributes, now setting callbacks");
        // Set the callbacks after creation
        XVaNestedList preedit_attr = preeditCallbacksList();
        XVaNestedList status_attr = statusCallbacksList();
        XSetICValues(activeic,
                     XNPreeditAttributes, preedit_attr,
                     XNStatusAttributes, status_attr,
                     NULL);
        XFree(preedit_attr);
        XFree(status_attr);
    }
    
    XSync(display, False);  // Force error handling
    
    CILog("XCreateIC returned: %p, x11_error_code: %d", activeic, x11_error_code);
    
    if (activeic == NULL || x11_error_code != 0) {
        CIError("Failed to create activeic with XIMPreeditCallbacks (error code: %d)", x11_error_code);
        // Just use another instance with supported style
        activeic = XCreateIC(
            xim,
            XNClientWindow,
            (Window)xwindow,
            XNFocusWindow,
            (Window)xwindow,
            XNInputStyle,
            XIMPreeditNone | XIMStatusNone,
            NULL
        );
        if (activeic == NULL) {
            CIError("Failed to create fallback activeic");
            return;
        }
        CILog("Created fallback activeic with XIMPreeditNone | XIMStatusNone");
    } else {
        // Verify the IC was actually created properly
        unsigned long actual_style = 0;
        XVaNestedList ic_values = XVaCreateNestedList(0, XNInputStyle, &actual_style, NULL);
        char *failed = XGetICValues(activeic, XNInputStyle, &actual_style, NULL);
        if (ic_values) XFree(ic_values);
        
        if (failed != NULL) {
            CIError("Failed to get IC values: %s", failed);
            // Don't destroy it - it might still work
        } else {
            CILog("Active IC created with style: 0x%lx (requested: 0x%lx)", 
                  (unsigned long)actual_style, 
                  (unsigned long)(XIMPreeditCallbacks | XIMStatusNothing));
        }
        
        // Even if we can't verify the style, keep the IC if it was created
        CILog("Keeping activeic even if style verification failed");
    }
    
    // If activeic failed, create a duplicate of inactiveic
    if (activeic == NULL) {
        activeic = XCreateIC(
            xim,
            XNClientWindow,
            (Window)xwindow,
            XNFocusWindow,
            (Window)xwindow,
            XNInputStyle,
            XIMPreeditNone | XIMStatusNone,
            NULL
        );
        if (activeic == NULL) {
            CIError("Failed to create any activeic");
            XDestroyIC(inactiveic);
            return;
        }
        CILog("Using duplicate IC for activeic");
    }
    
    XSetICFocus(inactiveic);
    if (activeic != NULL && activeic != inactiveic) {
        XUnsetICFocus(activeic);
    }
    CILog("Completed ic focus");
    XDestroyIC(x11c->ic);
    x11c->ic = inactiveic;
    CILog("Destroyed glfw ic");
    
    // Restore original error handler
    XSetErrorHandler(old_handler);
    
    CILog("CocoaInput X11 initializer done!");
}

void set_focus(int flag) {
    XUnsetICFocus(x11c->ic);
    if (flag) {
        x11c->ic = activeic;
    } else {
        x11c->ic = inactiveic;
    }
    XSetICFocus(x11c->ic);
    CIDebug("setFocused:%d", flag);
}