// SPDX-License-Identifier: Apache-2.0
#include <caf/all.hpp>
#include <caf/io/middleman.hpp>

#include "xstudio/atoms.hpp"
#include "xstudio/utility/serialise_headers.hpp"
#include "xstudio/caf_utility/caf_setup.hpp"
#include "xstudio/global/global_actor.hpp"
#include "xstudio/media/media_actor.hpp"
#include "xstudio/media_reader/media_reader.hpp"
#include "xstudio/utility/helpers.hpp"
#include "xstudio/utility/logging.hpp"
#include "xstudio/utility/caf_helpers.hpp"
#include "xstudio/playhead/playhead_actor.hpp"

#include "xstudio/ui/keyboard.hpp"
#include "xstudio/ui/mouse.hpp"
#include "xstudio/ui/opengl/opengl_viewport_renderer.hpp"
#include "xstudio/ui/viewport/viewport.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <iostream>

#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(
    Display *, GLXFBConfig, GLXContext, Bool, const int *);

using namespace xstudio;
using namespace xstudio::ui;
using namespace xstudio::ui::viewport;
using namespace xstudio::ui::opengl;

// Helper to check for extension string presence.  Adapted from:
//   http://www.opengl.org/resources/features/OGLextensions/
static bool isExtensionSupported(const char *extList, const char *extension) {
    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    /* It takes a bit of care to be fool-proof about parsing the
       OpenGL extensions string. Don't be fooled by sub-strings,
       etc. */
    for (start = extList;;) {
        where = strstr(start, extension);

        if (!where)
            break;

        terminator = where + strlen(extension);

        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return true;

        start = terminator;
    }

    return false;
}

static bool ctxErrorOccurred = false;
static int ctxErrorHandler(Display *dpy, XErrorEvent *ev) {
    ctxErrorOccurred = true;
    return 0;
}

static bool gl_context_current = false;

class GLXWindowViewportActor : public caf::event_based_actor {
  public:
    GLXWindowViewportActor(caf::actor_config &cfg);

    ~GLXWindowViewportActor();

    void execute();

    void set_playhead(caf::actor playhead) { viewport_renderer->set_playhead(playhead); }

  private:
    bool needs_redraw = {false};

    void create_glx_window();

    void event_loop_func();

    void close();

    void resizeGL(int w, int h);

    void render() {

        if (!ctx)
            return;

        glXMakeCurrent(display, win, ctx);

        // call vital init func
        viewport_renderer->init();

        // run a tight loop, rendering the viewport as fast as the system
        // is able. Accurate syncing with playhead refresh is still TODO for
        // this basic demo.
        while (ctx) {
            static auto then = utility::clock::now();
            glViewport(0, 0, width, height);
            viewport_renderer->render();
            auto n = utility::clock::now();
            std::cerr << "Redraw interval / microseconds: "
                      << std::chrono::duration_cast<std::chrono::microseconds>(
                             utility::clock::now() - then)
                             .count()
                      << "\n";
            then = n;
            glXSwapBuffers(display, win);

            // this is crucial for video refresh sync, the viewport
            // needs to know when the image was put on the screen
            viewport_renderer->framebuffer_swapped(utility::clock::now());
        }

        glXMakeCurrent(display, 0, 0); // releases the context so that this function can be
                                       // called from any thread by caf
    }

    void receive_change_notification(viewport::Viewport::ChangeCallbackId) {}

    Viewport *viewport_renderer;
    GLXContext ctx = 0;
    Display *display;
    Window win;
    Colormap cmap;
    std::thread event_loop_thread_;
    std::thread render_loop_thread_;
    int width  = 800;
    int height = 800;
};


static GLXWindowViewportActor *foofoo = nullptr;

GLXWindowViewportActor::GLXWindowViewportActor(caf::actor_config &cfg)
    : caf::event_based_actor(cfg) {

    // create the GLX window
    create_glx_window();

    utility::JsonStore jsn;
    jsn["base"] = utility::JsonStore();

    /* Here we create the xstudio OpenGLViewportRenderer class that actually draws video to the
     * screen
     */
    viewport_renderer = new ui::viewport::Viewport(
        jsn,
        caf::actor_cast<caf::actor>(this),
        true,
        ui::viewport::ViewportRendererPtr(new opengl::OpenGLViewportRenderer(true, false)),
        "GLXViewport");

    /* Provide a callback so the xstudio OpenGLViewportRenderer can tell this class when some
    property of the viewport has changed, or a redraw is needed, so the window
    can enact a refresh/redraw (although we don't use it in this example as
    the window is redrawing itself in a tight loop) */
    auto callback = [this](auto &&PH1) {
        receive_change_notification(std::forward<decltype(PH1)>(PH1));
    };
    viewport_renderer->set_change_callback(callback);

    // The OpenGLViewportRenderer class is not an 'actor' and therefore cannot directly
    // participate in the caf actor ssystem, sending and receiving messages. That's why this
    // GLXWindowViewportActor class is needed - we wrap the OpenGLViewportRenderer and expose
    // its message handler using the caf::actor::become() method. The reason the
    // OpenGLViewportRenderer is not an actor itself is to do with the way we have to 'mixin'
    // actors and QtObjects for the QT based UI. We go along with the pattern in this example
    // even though it's not QT and we the OpenGLViewportRenderer could be an actor itself.
    become(viewport_renderer->message_handler());

    resizeGL(800, 800);

    execute();
}

GLXWindowViewportActor::~GLXWindowViewportActor() {

    render_loop_thread_.join();
    event_loop_thread_.join();
    close();
    std::cerr << "window destroyed\n";
}

void GLXWindowViewportActor::execute() {

    render_loop_thread_ = std::thread(&GLXWindowViewportActor::render, this);
    event_loop_thread_  = std::thread(&GLXWindowViewportActor::event_loop_func, this);
}

void GLXWindowViewportActor::resizeGL(int w, int h) {
    width  = w;
    height = h;
    anon_send(
        caf::actor_cast<caf::actor>(this),
        ui::viewport::viewport_set_scene_coordinates_atom_v,
        Imath::V2f(0, 0),
        Imath::V2f(w, 0),
        Imath::V2f(w, h),
        Imath::V2f(0, h),
        Imath::V2i(w, h));
}

int main(int argc, char *argv[]) {

    using namespace xstudio;

    // utility::start_logger(spdlog::level::debug);

    ACTOR_INIT_GLOBAL_META()


    caf::core::init_global_meta_objects();
    caf::io::middleman::init_global_meta_objects();

    // As far as I can tell caf only allows config to be modified
    // through cli args. We prefer the 'sharing' policy rather than
    // 'stealing'. The latter results in multiple threads spinning
    // aggressively pushing CPU load very high during playback.
    const char *args[] = {argv[0], "--caf.scheduler.policy=sharing"};
    caf_utility::caf_config config{2, const_cast<char **>(args)};

    caf::actor_system system{config};

    // we need a handle on the system actor which we use to spawn components
    // (actors), or send and receive messages here to set-up the xstudio session
    caf::scoped_actor self{system};

    // Create the global actor that manages highest level objects
    caf::actor global_actor = self->spawn<global::GlobalActor>();

    // Tell the global_actor to create the 'studio' top level object that manages the 'session'
    // object(s)
    utility::request_receive<caf::actor>(
        *self, global_actor, global::create_studio_atom_v, "XStudio");

    // Create a session object
    auto session =
        utility::request_receive<caf::actor>(*self, global_actor, session::session_atom_v);


    caf::actor viewport_actor = self->spawn<GLXWindowViewportActor>();

    caf::actor playhead = self->spawn<playhead::PlayheadActor>("Playhead");

    std::vector<caf::actor> media_actors;
    for (int cli_source_idx = 1; cli_source_idx < argc; cli_source_idx++) {

        // create a MediaSourceActor for the source passed in cli
        utility::Uuid uuid      = utility::Uuid::generate();
        caf::actor media_source = self->spawn<media::MediaSourceActor>(
            "MediaSource1",
            utility::posix_path_to_uri(argv[cli_source_idx]),
            utility::FrameRate(timebase::k_flicks_24fps),
            uuid);

        // UuidActor is a std pair of an actor and its uuid
        auto media_source_and_uuid = utility::UuidActor(uuid, media_source);

        // media actor wraps 1 or more media sources
        caf::actor media_actor = self->spawn<media::MediaActor>(
            "Media1",
            utility::Uuid::generate(),
            utility::UuidActorVector({media_source_and_uuid}));

        media_actors.push_back(media_actor);
    }

    // set the incoming media actor as the source for the playhead
    utility::request_receive<bool>(*self, playhead, playhead::source_atom_v, media_actors);

    // set the playhead to string mode, so sources are strung together as a simple edit list
    self->send(
        playhead,
        module::change_attribute_value_atom_v,
        std::string("Compare"),
        utility::JsonStore("String"),
        true);

    // pass the playhead to the viewport - it will 'attach' itself to the playhead
    // so that it is receiving video frames for display
    utility::request_receive_wait<bool>(
        *self,
        viewport_actor,
        std::chrono::milliseconds(2000),
        viewport::viewport_playhead_atom_v,
        "viewport0",
        playhead);

    // start playback
    self->send(playhead, playhead::play_atom_v, true);

    system.await_actors_before_shutdown(true);

    return 0;
}

void GLXWindowViewportActor::create_glx_window() {

    XInitThreads();

    display = XOpenDisplay(nullptr);

    if (!display) {
        printf("Failed to open X display\n");
        exit(1);
    }

    // Get a matching FB config
    static int visual_attribs[] = {
        GLX_X_RENDERABLE,
        True,
        GLX_DRAWABLE_TYPE,
        GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,
        GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,
        GLX_TRUE_COLOR,
        GLX_RED_SIZE,
        8,
        GLX_GREEN_SIZE,
        8,
        GLX_BLUE_SIZE,
        8,
        GLX_ALPHA_SIZE,
        8,
        GLX_DEPTH_SIZE,
        24,
        GLX_STENCIL_SIZE,
        8,
        GLX_DOUBLEBUFFER,
        True,
        // GLX_SAMPLE_BUFFERS  , 1,
        // GLX_SAMPLES         , 4,
        None};

    int glx_major, glx_minor;

    // FBConfigs were added in GLX version 1.3.
    if (!glXQueryVersion(display, &glx_major, &glx_minor) ||
        ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1)) {
        printf("Invalid GLX version");
        exit(1);
    }

    printf("Getting matching framebuffer configs\n");
    int fbcount;
    GLXFBConfig *fbc =
        glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc) {
        printf("Failed to retrieve a framebuffer config\n");
        exit(1);
    }
    printf("Found %d matching FB configs.\n", fbcount);

    // Pick the FB config/visual with the most samples per pixel
    printf("Getting XVisualInfos\n");
    int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;

    int i;
    for (i = 0; i < fbcount; ++i) {
        XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[i]);
        if (vi) {
            int samp_buf, samples;
            glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
            glXGetFBConfigAttrib(display, fbc[i], GLX_SAMPLES, &samples);

            printf(
                "  Matching fbconfig %d, visual ID 0x%2x: SAMPLE_BUFFERS = %d,"
                " SAMPLES = %d\n",
                i,
                vi->visualid,
                samp_buf,
                samples);

            if (best_fbc < 0 || samp_buf && samples > best_num_samp)
                best_fbc = i, best_num_samp = samples;
            if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
                worst_fbc = i, worst_num_samp = samples;
        }
        XFree(vi);
    }

    GLXFBConfig bestFbc = fbc[best_fbc];

    // Be sure to free the FBConfig list allocated by glXChooseFBConfig()
    XFree(fbc);

    // Get a visual
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, bestFbc);
    printf("Chosen visual ID = 0x%x\n", vi->visualid);

    printf("Creating colormap\n");
    XSetWindowAttributes swa;
    swa.colormap = cmap =
        XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.background_pixmap = None;
    swa.border_pixel      = 0;
    swa.event_mask = StructureNotifyMask | PointerMotionMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask;

    printf("Creating window\n");
    win = XCreateWindow(
        display,
        RootWindow(display, vi->screen),
        0,
        0,
        800,
        800,
        0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWBorderPixel | CWColormap | CWEventMask,
        &swa);
    if (!win) {
        printf("Failed to create window.\n");
        exit(1);
    }

    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress(
        (const GLubyte *)"glXSwapIntervalEXT"); // Set the glxSwapInterval to 0, ie. disable
                                                // vsync!  khronos.org/opengl/wiki/Swap_Interval
    glXSwapIntervalEXT(display, win, 1);        // glXSwapIntervalEXT(0);

    // Done with the visual info data
    XFree(vi);

    XStoreName(display, win, "GL 3.0 Window");

    printf("Mapping window\n");
    XMapWindow(display, win);

    // Get the default screen's GLX extension list
    const char *glxExts = glXQueryExtensionsString(display, DefaultScreen(display));

    // NOTE: It is not necessary to create or make current to a context before
    // calling glXGetProcAddressARB
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB(
        (const GLubyte *)"glXCreateContextAttribsARB");


    // Install an X error handler so the application won't exit if GL 3.0
    // context allocation fails.
    //
    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    ctxErrorOccurred                            = false;
    int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(&ctxErrorHandler);

    // Check for the GLX_ARB_create_context extension string and the function.
    // If either is not present, use GLX 1.3 context creation method.
    if (!isExtensionSupported(glxExts, "GLX_ARB_create_context") ||
        !glXCreateContextAttribsARB) {
        printf("glXCreateContextAttribsARB() not found"
               " ... using old-style GLX context\n");
        ctx = glXCreateNewContext(display, bestFbc, GLX_RGBA_TYPE, 0, True);
    }

    // If it does, try to get a GL 3.0 context!
    else {
        int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB,
            3,
            GLX_CONTEXT_MINOR_VERSION_ARB,
            0,
            // GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            None};

        printf("Creating context\n");
        ctx = glXCreateContextAttribsARB(display, bestFbc, 0, True, context_attribs);

        // Sync to ensure any errors generated are processed.
        XSync(display, False);
        if (!ctxErrorOccurred && ctx)
            printf("Created GL 3.0 context\n");
        else {
            // Couldn't create GL 3.0 context.  Fall back to old-style 2.x context.
            // When a context version below 3.0 is requested, implementations will
            // return the newest context version compatible with OpenGL versions less
            // than version 3.0.
            // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
            context_attribs[1] = 1;
            // GLX_CONTEXT_MINOR_VERSION_ARB = 0
            context_attribs[3] = 0;

            ctxErrorOccurred = false;

            printf("Failed to create GL 3.0 context"
                   " ... using old-style GLX context\n");
            ctx = glXCreateContextAttribsARB(display, bestFbc, 0, True, context_attribs);
        }
    }

    // Sync to ensure any errors generated are processed.
    XSync(display, False);

    // Restore the original error handler
    XSetErrorHandler(oldHandler);

    if (ctxErrorOccurred || !ctx) {
        printf("Failed to create an OpenGL context\n");
        exit(1);
    }

    // Verifying that context is a direct context
    if (!glXIsDirect(display, ctx)) {
        printf("Indirect GLX rendering context obtained\n");
    } else {
        printf("Direct GLX rendering context obtained\n");
    }
    // glXMakeCurrent( display, 0, 0 );
}


void GLXWindowViewportActor::event_loop_func() {

    std::cerr << "Event loop\n";
    int windowOpen = 1;
    // a caf 'actor' handle to this class, se we can send messages to ourself
    auto self = caf::actor_cast<caf::actor>(this);
    while (windowOpen) {
        XEvent ev = {};

        while (XPending(display) > 0) {
            XNextEvent(display, &ev);

            // see viewport.cpp ... this message will cause a redraw
            anon_send(self, show_buffer_atom_v, true);
            // render();

            switch (ev.type) {
            case DestroyNotify: {
                XDestroyWindowEvent *e = (XDestroyWindowEvent *)&ev;
                if (e->window == win) {
                    windowOpen = 0;
                }
            } break;
            case ConfigureNotify: {
                XConfigureEvent *e = (XConfigureEvent *)&ev;
                if (e->window == win) {
                    resizeGL(e->width, e->height);
                }
            } break;
            }
        }
    }
    std::cerr << "bye\n";
    ctx = 0;
}

void GLXWindowViewportActor::close() {

    glXMakeCurrent(display, 0, 0);
    glXDestroyContext(display, ctx);

    XDestroyWindow(display, win);
    XFreeColormap(display, cmap);
    XCloseDisplay(display);
}