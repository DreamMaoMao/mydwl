/*
 * See LICENSE file for copyright and license details.
 */
#define XWAYLNAD 1
#include <getopt.h>
#include <libinput.h>
#include <limits.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/util/region.h>
// #include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_input_inhibitor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xdg_foreign_registry.h>
#include <wlr/types/wlr_xdg_foreign_v1.h>
#include <wlr/types/wlr_xdg_foreign_v2.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include <X11/Xlib.h>
#include <xcb/xcb_icccm.h>
#endif

#include "dwl-ipc-unstable-v2-protocol.h"
#include "util.h"
#include "wlr_foreign_toplevel_management_v1.h"


/* macros */
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define ISFULLSCREEN(A)			((A)->isfullscreen || (A)->isfakefullscreen || (A)->isrealfullscreen || (A)->overview_isfakefullscreenbak || (A)->overview_isfullscreenbak || (A)->overview_isrealfullscreenbak)
#define LISTEN_STATIC(E, H)     do { static struct wl_listener _l = {.notify = (H)}; wl_signal_add((E), &_l); } while (0)

/* enums */
/* enums */
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 }; /* client types */
enum { AxisUp, AxisRight, AxisDown, AxisLeft }; //滚轮滚动的方向
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrFS, LyrTop, LyrOverlay,
#ifdef IM
       LyrIMPopup,
#endif
       LyrBlock, NUM_LAYERS }; /* scene layers */
#ifdef XWAYLAND
enum { NetWMWindowTypeDialog, NetWMWindowTypeSplash, NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility, NetLast }; /* EWMH atoms */
#endif
enum { UP, DOWN, LEFT, RIGHT };                  /* movewin */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button; //鼠标按键

typedef struct {
	unsigned int mod;
	unsigned int dir;
	void (*func)(const Arg *);
	const Arg arg;
} Axis;

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom,oldgeom;  /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener minimize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
	struct wlr_box prev;  /* layout-relative, includes border */
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
	unsigned int bw;
	unsigned int tags,oldtags;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel;
	int isfloating, isurgent, isfullscreen, istiled, isminied;
	int isfakefullscreen,isrealfullscreen;
  	int overview_backup_x, overview_backup_y, overview_backup_w,
  	    overview_backup_h, overview_backup_bw;
  	int fullscreen_backup_x, fullscreen_backup_y, fullscreen_backup_w,
  	    fullscreen_backup_h;
	int overview_isfullscreenbak,overview_isfakefullscreenbak,overview_isrealfullscreenbak,overview_isfloatingbak;
	uint32_t resize; /* configure serial of a pending resize */

	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener foreign_activate_request;
	struct wl_listener foreign_fullscreen_request;
	struct wl_listener foreign_close_request;
	struct wl_listener foreign_destroy;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;	

	unsigned int ignore_clear_fullscreen;
	int isnoclip;
	int is_in_scratchpad;
	int is_scratchpad_show;
	int isglobal;
	int isnoborder;
	int iskilling;
	struct wlr_box bounds;
} Client;

typedef struct {
    struct wl_list link;
    struct wl_resource *resource;
    Monitor *monitor;
} DwlIpcOutput;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wl_list link;
	struct wlr_keyboard *wlr_keyboard;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods; /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} Keyboard;

typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
} LayerSurface;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *,unsigned int,unsigned int);
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m; /* monitor area, layout-relative */
	struct wlr_box w; /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface::link */
	const Layout *lt[2];
	unsigned int seltags;
	unsigned int sellt;
	uint32_t tagset[2];
	double mfact;
	int nmaster;
	char ltsymbol[16];

    struct wl_list dwl_ipc_outputs;
    int gappih;           /* horizontal gap between windows */
    int gappiv;           /* vertical gap between windows */
    int gappoh;           /* horizontal outer gaps */
    int gappov;           /* vertical outer gaps */
	Pertag *pertag;
	Client *sel;
	int isoverview;
	int is_in_hotarea;
	int gamma_lut_changed;
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

typedef struct {
	struct wlr_pointer_constraint_v1 *constraint;
	struct wl_listener destroy;
} PointerConstraint;

typedef struct {
	const char *id;
	const char *title;
	unsigned int tags;
	int isfloating;
	int isfullscreen;
	int isnoclip;
	int isnoborder;
	int monitor;
	unsigned int width;
	unsigned int height;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* function declarations */
static void logtofile(const char *fmt, ...); //日志函数
static void lognumtofile(unsigned int num);  //日志函数
static void applybounds(Client *c, struct wlr_box *bbox); //设置边界规则,能让一些窗口拥有比较适合的大小
static void applyrules(Client *c); //窗口规则应用,应用config.h中定义的窗口规则
static void arrange(Monitor *m); //布局函数,让窗口俺平铺规则移动和重置大小
static void arrangelayer(Monitor *m, struct wl_list *list, 
struct wlr_box *usable_area, int exclusive);
static void arrangelayers(Monitor *m);
static void autostartexec(void);  //自启动命令执行
static void axisnotify(struct wl_listener *listener, void *data);  //滚轮事件处理
static void buttonpress(struct wl_listener *listener, void *data); //鼠标按键事件处理
static void chvt(const Arg *arg); 
static void checkidleinhibitor(struct wlr_surface *exclude);
static void cleanup(void);  //退出清理
static void cleanupkeyboard(struct wl_listener *listener, void *data); //退出清理
static void cleanupmon(struct wl_listener *listener, void *data); //退出清理
static void closemon(Monitor *m); //退出清理
static void toggle_hotarea(int x_root, int y_root); //触发热区
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitnotify(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createidleinhibitor(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static void requestmonstate(struct wl_listener *listener, void *data);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createlocksurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void destroydecoration(struct wl_listener *listener, void *data);
static void defaultgaps(const Arg *arg);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroyidleinhibitor(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylock(SessionLock *lock, int unlocked);
static void destroylocksurface(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void destroysessionlock(struct wl_listener *listener, void *data);
static void destroysessionmgr(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void setcursorshape(struct wl_listener *listener, void *data);
static void dwl_ipc_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
static void dwl_ipc_manager_destroy(struct wl_resource *resource);
static void dwl_ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output);
static void dwl_ipc_manager_release(struct wl_client *client, struct wl_resource *resource);
static void dwl_ipc_output_destroy(struct wl_resource *resource);
static void dwl_ipc_output_printstatus(Monitor *monitor);
static void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output);
static void dwl_ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags);
static void dwl_ipc_output_set_layout(struct wl_client *client, struct wl_resource *resource, uint32_t index);
static void dwl_ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset);
static void dwl_ipc_output_release(struct wl_client *client, struct wl_resource *resource);
static void focusclient(Client *c, int lift);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void setborder_color(Client *c);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void incnmaster(const Arg *arg);
static void incgaps(const Arg *arg);
static void incigaps(const Arg *arg);
static int keyrepeat(void *data);
static void incihgaps(const Arg *arg);
static void incivgaps(const Arg *arg);
static void incogaps(const Arg *arg);
static void incohgaps(const Arg *arg);
static void incovgaps(const Arg *arg);
static void toggleglobal(const Arg *arg);
static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(uint32_t mods, xkb_keysym_t sym);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static void killclient(const Arg *arg);
static void locksession(struct wl_listener *listener, void *data);
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void minimizenotify(struct wl_listener *listener, void *data);
static void monocle(Monitor *m);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(uint32_t time, struct wlr_input_device *device, double sx,
		double sy, double sx_unaccel, double sy_unaccel);
static void motionrelative(struct wl_listener *listener, void *data);
static void moveresize(const Arg *arg);
static void exchange_client(const Arg *arg);
static void reset_foreign_tolevel(Client *c);
static void exchange_two_client(Client *c1, Client *c2);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void pointerfocus(Client *c, struct wlr_surface *surface,
		double sx, double sy, uint32_t time);
static void printstatus(void);
static void quit(const Arg *arg);
static void quitsignal(int signo);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int interact);
static void resizeclient(Client *c,int x,int y,int w,int h, int interact);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int floating);
static void setfullscreen(Client *c, int fullscreen);
static void setfakefullscreen(Client *c, int fakefullscreen);
static void setrealfullscreen(Client *c, int realfullscreen);
static void setgaps(int oh, int ov, int ih, int iv);
static void setlayout(const Arg *arg);
static void switch_layout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setmon(Client *c, Monitor *m, unsigned int newtags);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void startdrag(struct wl_listener *listener, void *data);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void setgamma(struct wl_listener *listener, void *data);
static void tile(Monitor *m, unsigned int gappo, unsigned int uappi);
static void overview(Monitor *m, unsigned int gappo, unsigned int gappi);
static void grid(Monitor *m, unsigned int gappo, unsigned int uappi);
static void togglefloating(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void togglefakefullscreen(const Arg *arg);
static void togglerealfullscreen(const Arg *arg);
static void togglegaps(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unlocksession(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void updatetitle(struct wl_listener *listener, void *data);
static void urgent(struct wl_listener *listener, void *data);
static void view(const Arg *arg);
static void viewtoleft_have_client(const Arg *arg);
static void viewtoright_have_client(const Arg *arg);
static void viewtoleft(const Arg *arg);
static void viewtoright(const Arg *arg);
static void handlesig(int signo);
static void tagtoleft(const Arg *arg);
static void tagtoright(const Arg *arg); 
static void virtualkeyboard(struct wl_listener *listener, void *data);
static void virtualpointer(struct wl_listener *listener, void *data);
static void warp_cursor(const Client *c);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny);
static void zoom(const Arg *arg);
static void clear_fullscreen_flag(Client *c);
static Client *direction_select(const Arg *arg);
static void focusdir(const Arg *arg);
static void toggleoverview(const Arg *arg);
static void warp_cursor_to_selmon(const Monitor *m);
unsigned int want_restore_fullscreen(Client *target_client);
static void overview_restore(Client *c, const Arg *arg);
static void overview_backup(Client *c);
static void applyrulesgeom(Client *c);
static void set_minized(Client *c);
static void minized(const Arg *arg);
static void restore_minized(const Arg *arg);
static void toggle_scratchpad(const Arg *arg);
static void show_scratchpad(Client *c);
static void show_hide_client(Client *c);
static void tag_client(const Arg *arg, Client *target_client);

static void handle_foreign_activate_request(struct wl_listener *listener, void *data);
static void handle_foreign_fullscreen_request(struct wl_listener *listener, void *data);
static void handle_foreign_close_request(struct wl_listener *listener, void *data);
static void handle_foreign_destroy(struct wl_listener *listener, void *data);

static struct wlr_box setclient_coordinate_center(struct wlr_box geom);
static unsigned int get_tags_first_tag(unsigned int tags);
/* variables */
static const char broken[] = "broken";
// static const char *cursor_image = "left_ptr";
static pid_t child_pid = -1;
static int locked;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wlr_relative_pointer_manager_v1 *pointer_manager;
static struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
static struct wlr_backend *backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;

static struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients; /* tiling order */
static struct wl_list fstack;  /* focus order */
// static struct wlr_idle *idle;
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
// static struct wlr_input_inhibit_manager *input_inhibit_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;
static struct wlr_session *session;

static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };
static struct wlr_scene_tree *drag_icon;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr; //这个跟steup obs影响对应
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;

static struct wlr_seat *seat;
static struct wl_list keyboards;
static unsigned int cursor_mode;
static Client *grabc;
static int grabcx, grabcy; /* client-relative */

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;

static int enablegaps = 1;   /* enables gaps, used by togglegaps */

/* global event handlers */
static struct zdwl_ipc_manager_v2_interface dwl_manager_implementation = {.release = dwl_ipc_manager_release, .get_output = dwl_ipc_manager_get_output};
static struct zdwl_ipc_output_v2_interface dwl_output_implementation = {.release = dwl_ipc_output_release, .set_tags = dwl_ipc_output_set_tags, .set_layout = dwl_ipc_output_set_layout, .set_client_tags = dwl_ipc_output_set_client_tags};
static struct wl_listener lock_listener = {.notify = locksession};

#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static Atom getatom(xcb_connection_t *xc, const char *name);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
// static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
// static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
static Atom netatom[NetLast];
#endif

/* configuration, allows nested code to access above variables */
#include "config.h"

/* attempt to encapsulate suck into one file */
#include "client.h"
#ifdef IM
#include "IM.h"
#endif

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
	float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
	unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
	const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
};

static pid_t *autostart_pids;
static size_t autostart_len;

void //0.5
applybounds(Client *c, struct wlr_box *bbox)
{
	/* set minimum possible */
	c->geom.width = MAX(1 + 2 * (int)c->bw, c->geom.width);
	c->geom.height = MAX(1 + 2 * (int)c->bw, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height <= bbox->y)
		c->geom.y = bbox->y;
}

/*清除全屏标志,还原全屏时清0的border*/
void clear_fullscreen_flag(Client *c) {
  if (c->isfullscreen || c->isfakefullscreen || c->isrealfullscreen ) {
    c->isfullscreen = 0;
    c->isfakefullscreen = 0;
    c->isrealfullscreen = 0;
    c->bw = borderpx;
	client_set_fullscreen(c, false);
  }
}

void //升级忽略
setgamma(struct wl_listener *listener, void *data)
{
	struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	if (!wlr_gamma_control_v1_apply(event->control, &state)) {
		wlr_output_state_finish(&state);
		return;
	}

	if (!wlr_output_test_state(event->output, &state)) {
		wlr_gamma_control_v1_send_failed_and_destroy(event->control);
		wlr_output_state_finish(&state);
		return;
	}

	wlr_output_commit_state(event->output, &state);
	wlr_output_schedule_frame(event->output);
}

void minized(const Arg *arg) {
	if(selmon->sel && !selmon->sel->isminied) {
		set_minized(selmon->sel);
	}
}

void restore_minized(const Arg *arg) {
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if (c->isminied) {
			show_hide_client(c);
			c->is_scratchpad_show = 0;
			c->is_in_scratchpad = 0;
			setborder_color(c);
			break;
		}
	}

}

void show_scratchpad(Client *c) {
	c->is_scratchpad_show = 1;
  	if (c->isfullscreen || c->isfakefullscreen ||c->isrealfullscreen ) {
  		c->isfullscreen = 0; // 清除窗口全屏标志
		c->isfakefullscreen = 0;
		c->isrealfullscreen = 0;
		c->bw = borderpx; // 恢复非全屏的border
  	}
	/* return if fullscreen */
	if(!c->isfloating) {
		setfloating(c, 1);	
		c->geom.width =  c->mon->w.width * 0.7;
		c->geom.height =  c->mon->w.height * 0.8;		
		//重新计算居中的坐标
		c->geom = setclient_coordinate_center(c->geom);		
		resize(c, c->geom, 0);
	}
	c->oldtags = selmon->tagset[selmon->seltags];
	show_hide_client(c);
	setborder_color(c);
}

void reset_foreign_tolevel(Client *c) {
	wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
	c->foreign_toplevel = NULL;
	// printstatus();
	//创建外部顶层窗口的句柄,每一个顶层窗口都有一个
	c->foreign_toplevel = wlr_foreign_toplevel_handle_v1_create(foreign_toplevel_manager);
	//监听来自外部对于窗口的事件请求
	if(c->foreign_toplevel){
		LISTEN(&(c->foreign_toplevel->events.request_activate),
				&c->foreign_activate_request,handle_foreign_activate_request);
		LISTEN(&(c->foreign_toplevel->events.request_fullscreen),
				&c->foreign_fullscreen_request,handle_foreign_fullscreen_request);
		LISTEN(&(c->foreign_toplevel->events.request_close),
				&c->foreign_close_request,handle_foreign_close_request);
		LISTEN(&(c->foreign_toplevel->events.destroy),
				&c->foreign_destroy,handle_foreign_destroy);
		//设置外部顶层句柄的id为应用的id
		const char *appid;
    	appid = client_get_appid(c) ;
		if(appid)
			wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign_toplevel,appid);
		//设置外部顶层句柄的title为应用的title
		const char *title;
    	title = client_get_title(c) ;
		if(title)
			wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel,title);
		//设置外部顶层句柄的显示监视器为当前监视器
		wlr_foreign_toplevel_handle_v1_output_enter(
				c->foreign_toplevel, c->mon->wlr_output);
	}
}

void toggle_scratchpad(const Arg *arg) {
	Client *c;
	wl_list_for_each(c, &clients, link) {
		if(c->mon != selmon) {
			continue;
		}
		if(c->is_in_scratchpad && c->is_scratchpad_show && (selmon->tagset[selmon->seltags] & c->tags) == 0 ) {
			unsigned int target = get_tags_first_tag(selmon->tagset[selmon->seltags]); 
			tag_client(&(Arg){.ui = target},c);
			return;
		} else if (c->is_in_scratchpad && c->is_scratchpad_show && (selmon->tagset[selmon->seltags] & c->tags) != 0) {
			set_minized(c);
			return;
		} else if ( c && c->is_in_scratchpad && !c->is_scratchpad_show) {
			show_scratchpad(c);
			return;
		} 
	}
}

void  //0.5
handlesig(int signo)
{
	if (signo == SIGCHLD) {
#ifdef XWAYLAND
		siginfo_t in;
		/* wlroots expects to reap the XWayland process itself, so we
		 * use WNOWAIT to keep the child waitable until we know it's not
		 * XWayland.
		 */
		while (!waitid(P_ALL, 0, &in, WEXITED|WNOHANG|WNOWAIT) && in.si_pid
				&& (!xwayland || in.si_pid != xwayland->server->pid))
			waitpid(in.si_pid, NULL, 0);
#else
		while (waitpid(-1, NULL, WNOHANG) > 0);
#endif
	} else if (signo == SIGINT || signo == SIGTERM) {
		quit(NULL);
	}
}

void toggle_hotarea(int x_root, int y_root) {
  // 左下角热区坐标计算,兼容多显示屏
  Arg arg = {0};
  unsigned hx = selmon->m.x + hotarea_size;
  unsigned hy = selmon->m.y + selmon->m.height - hotarea_size;

  if (enable_hotarea == 1 && selmon->is_in_hotarea == 0 && y_root > hy &&
      x_root < hx && x_root >= selmon->m.x &&
      y_root <= (selmon->m.y + selmon->m.height)) {
    toggleoverview(&arg);
    selmon->is_in_hotarea = 1;
  } else if (enable_hotarea == 1 && selmon->is_in_hotarea == 1 &&
             (y_root <= hy || x_root >= hx || x_root < selmon->m.x ||
              y_root > (selmon->m.y + selmon->m.height))) {
    selmon->is_in_hotarea = 0;
  }
}

struct wlr_box //计算客户端居中坐标
setclient_coordinate_center(struct wlr_box geom){
	struct wlr_box tempbox;
	tempbox.x = selmon->w.x + (selmon->w.width - geom.width) / 2;
	tempbox.y = selmon->w.y + (selmon->w.height - geom.height) / 2;
	tempbox.width = geom.width;
	tempbox.height = geom.height;
	return tempbox;

}

/* function implementations */
void logtofile(const char *fmt, ...) {
  char buf[256];
  char cmd[256];
  va_list ap;
  va_start(ap, fmt);
  vsprintf((char *)buf, fmt, ap);
  va_end(ap);
  unsigned int i = strlen((const char *)buf);

  sprintf(cmd, "echo '%.*s' >> ~/log", i, buf);
  system(cmd);
}

/* function implementations */
void lognumtofile(unsigned int num) {
  char cmd[256];
  sprintf(cmd, "echo '%x' >> ~/log", num);
  system(cmd);
}

void // 0.5
applyrulesgeom(Client *c)
{
	/* rule matching */
	const char *appid, *title;
	const Rule *r;

	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title))
				&& (!r->id || strstr(appid, r->id)) && r->width !=0 && r->height != 0) {	
			c->geom.width = r->width;
			c->geom.height =  r->height;
			//重新计算居中的坐标
			c->geom = setclient_coordinate_center(c->geom);
			break;
		}
	}
}

void // 17
applyrules(Client *c)
{
	/* rule matching */
	const char *appid, *title;
	uint32_t i, newtags = 0;
	const Rule *r;
	Monitor *mon = selmon, *m;

	c->isfloating = client_is_float_type(c);
	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title))
				&& (!r->id || strstr(appid, r->id))) {
			c->isfloating = r->isfloating;
			c->isnoclip = r->isnoclip;
			c->isnoborder = r->isnoborder;
			newtags |= r->tags;
			i = 0;
			wl_list_for_each(m, &mons, link)
				if (r->monitor == i++)
					mon = m;
					
			if(c->isfloating && r->width != 0 && r->height != 0){
				c->geom.width = r->width;
				c->geom.height =  r->height;
				//重新计算居中的坐标
				c->geom = setclient_coordinate_center(c->geom);
			}
			if(r->isfullscreen){
				c->isrealfullscreen =1;
				c->isfullscreen =1;
				c->ignore_clear_fullscreen = 1;
			}
			break;
		}
	}

	wlr_scene_node_reparent(&c->scene->node, layers[c->isfloating ? LyrFloat : LyrTile]);
	setmon(c, mon, newtags);

	Client *fc;
  	// 如果当前的tag中有新创建的非悬浮窗口,就让当前tag中的全屏窗口退出全屏参与平铺
	wl_list_for_each(fc, &clients, link)
  		if (fc && !c->ignore_clear_fullscreen && c->tags & fc->tags && ISFULLSCREEN(fc) && !c->isfloating ) {
  		  	clear_fullscreen_flag(fc);
			arrange(c->mon);
  		}else if(c->ignore_clear_fullscreen && c->isrealfullscreen){
			setrealfullscreen(c,1);
		}

	if(!(c->tags & ( 1 << (selmon->pertag->curtag - 1) ))){
		view(&(Arg){.ui = c->tags});
	}
}

void //17
arrange(Monitor *m)
{
	Client *c;

	if (!m->wlr_output->enabled)
		return;

	wl_list_for_each(c, &clients, link) {
		if(c->mon == m && c->isglobal) {
			c->tags =  m->tagset[m->seltags];
			focusclient(c,1);
		}
		if (c->mon == m) {
			wlr_scene_node_set_enabled(&c->scene->node, VISIBLEON(c, m));
			client_set_suspended(c, !VISIBLEON(c, m));
		}
	}

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
			(c = focustop(m)) && c->isfullscreen);



	if(m->isoverview){
		overviewlayout.arrange(m,0,0);
	}else if (m && m->lt[m->sellt]->arrange){
		m->lt[m->sellt]->arrange(m,gappoh,0);
	}

	#ifdef IM
		        if (input_relay && input_relay->popup)
			        input_popup_update(input_relay->popup);
	#endif
	motionnotify(0, NULL, 0, 0, 0, 0);
	checkidleinhibitor(NULL);
}

void //0.5
arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area, int exclusive)
{
	LayerSurface *l;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link) {
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area, usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x, l->scene->node.y);
		l->geom.x = l->scene->node.x;
		l->geom.y = l->scene->node.y;
	}
}

Client *direction_select(const Arg *arg) {
	Client *c,*tempClients[100];
	Client *tc = selmon->sel;
	int last = -1;

	if(!tc)
		return NULL;

	if (tc &&
	    (tc->isfullscreen || tc->isfakefullscreen || tc->isrealfullscreen )) /* no support for focusstack with fullscreen windows */
	  return NULL;

	wl_list_for_each(c, &clients, link)
		if (c && (c->tags & c->mon->tagset[c->mon->seltags])){
			last++;
	  		tempClients[last] = c;
		}
  	if (last < 0)
  	  return NULL;
  	int sel_x = tc->geom.x;
  	int sel_y = tc->geom.y;
  	long long int distance = LLONG_MAX;
  	// int temp_focus = 0;
  	Client *tempFocusClients = NULL;

  	switch (arg->i) {
  	case UP:
  	  for (int _i = 0; _i <= last; _i++) {
  	    if (tempClients[_i]->geom.y < sel_y && tempClients[_i]->geom.x == sel_x) {
  	      int dis_x = tempClients[_i]->geom.x - sel_x;
  	      int dis_y = tempClients[_i]->geom.y - sel_y;
  	      long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	      if (tmp_distance < distance) {
  	        distance = tmp_distance;
  	        tempFocusClients = tempClients[_i];
  	      }
  	    }
  	  }
	  if (!tempFocusClients) {
  	  	for (int _i = 0; _i <= last; _i++) {
  	  	  if (tempClients[_i]->geom.y < sel_y ) {
  	  	    int dis_x = tempClients[_i]->geom.x - sel_x;
  	  	    int dis_y = tempClients[_i]->geom.y - sel_y;
  	  	    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	  	    if (tmp_distance < distance) {
  	  	      distance = tmp_distance;
  	  	      tempFocusClients = tempClients[_i];
  	  	    }
  	  	  }
  	  	}
	  }
  	  break;
  	case DOWN:
  	  for (int _i = 0; _i <= last; _i++) {
  	    if (tempClients[_i]->geom.y > sel_y && tempClients[_i]->geom.x == sel_x) {
  	      int dis_x = tempClients[_i]->geom.x - sel_x;
  	      int dis_y = tempClients[_i]->geom.y - sel_y;
  	      long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	      if (tmp_distance < distance) {
  	        distance = tmp_distance;
  	        tempFocusClients = tempClients[_i];
  	      }
  	    }
  	  }
	  if (!tempFocusClients) {
  	  	for (int _i = 0; _i <= last; _i++) {
  	  	  if (tempClients[_i]->geom.y > sel_y ) {
  	  	    int dis_x = tempClients[_i]->geom.x - sel_x;
  	  	    int dis_y = tempClients[_i]->geom.y - sel_y;
  	  	    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	  	    if (tmp_distance < distance) {
  	  	      distance = tmp_distance;
  	  	      tempFocusClients = tempClients[_i];
  	  	    }
  	  	  }
  	  	}
	  }
  	  break;
  	case LEFT:
  	  for (int _i = 0; _i <= last; _i++) {
  	    if (tempClients[_i]->geom.x < sel_x && tempClients[_i]->geom.y == sel_y) {
  	      int dis_x = tempClients[_i]->geom.x - sel_x;
  	      int dis_y = tempClients[_i]->geom.y - sel_y;
  	      long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	      if (tmp_distance < distance) {
  	        distance = tmp_distance;
  	        tempFocusClients = tempClients[_i];
  	      }
  	    }
  	  }
	  if(!tempFocusClients) {
  	  	for (int _i = 0; _i <= last; _i++) {
  	  	  if (tempClients[_i]->geom.x < sel_x ) {
  	  	    int dis_x = tempClients[_i]->geom.x - sel_x;
  	  	    int dis_y = tempClients[_i]->geom.y - sel_y;
  	  	    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	  	    if (tmp_distance < distance) {
  	  	      distance = tmp_distance;
  	  	      tempFocusClients = tempClients[_i];
  	  	    }
  	  	  }
  	  	}
	  }
  	  break;
  	case RIGHT:
  	  for (int _i = 0; _i <= last; _i++) {
  	    if (tempClients[_i]->geom.x > sel_x && tempClients[_i]->geom.y == sel_y) {
  	      int dis_x = tempClients[_i]->geom.x - sel_x;
  	      int dis_y = tempClients[_i]->geom.y - sel_y;
  	      long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	      if (tmp_distance < distance) {
  	        distance = tmp_distance;
  	        tempFocusClients = tempClients[_i];
  	      }
  	    }
  	  }
	  if(!tempFocusClients) {
  	  	for (int _i = 0; _i <= last; _i++) {
  	  	  if (tempClients[_i]->geom.x > sel_x ) {
  	  	    int dis_x = tempClients[_i]->geom.x - sel_x;
  	  	    int dis_y = tempClients[_i]->geom.y - sel_y;
  	  	    long long int tmp_distance = dis_x * dis_x + dis_y * dis_y; // 计算距离
  	  	    if (tmp_distance < distance) {
  	  	      distance = tmp_distance;
  	  	      tempFocusClients = tempClients[_i];
  	  	    }
  	  	  }
  	  	}
	  }
  	  break;
  	}
  	return tempFocusClients;
}

void focusdir(const Arg *arg) {
	Client *c;
	c = direction_select(arg);
	if(c) {
		focusclient(c,1);
		if(warpcursor)
			warp_cursor(c);
	}
}

void //0.5
arrangelayers(Monitor *m)
{
	int i;
	struct wlr_box usable_area = m->m;
	LayerSurface *l;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	for (i = 0; i < (int)LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(l, &m->layers[layers_above_shell[i]], link) {
			if (locked || !l->layer_surface->current.keyboard_interactive || !l->mapped)
				continue;
			/* Deactivate the focused client. */
			focusclient(NULL, 0);
			exclusive_focus = l;
			client_notify_enter(l->layer_surface->surface, wlr_seat_get_keyboard(seat));
			return;
		}
	}
}



void
autostartexec(void) {
	const char *const *p;
	size_t i = 0;

	/* count entries */
	for (p = autostart; *p; autostart_len++, p++)
		while (*++p);

	autostart_pids = calloc(autostart_len, sizeof(pid_t));
	for (p = autostart; *p; i++, p++) {
		if ((autostart_pids[i] = fork()) == 0) {
			setsid();
			execvp(*p, (char *const *)p);
			die("dwl: execvp %s:", *p);
		}
		/* skip arguments */
		while (*++p);
	}
}

void //鼠标滚轮事件
axisnotify(struct wl_listener *listener, void *data)
{


	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	struct wlr_keyboard *keyboard;
	uint32_t mods;
	const Axis *a;
	unsigned int adir;
	// IDLE_NOTIFY_ACTIVITY;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
	keyboard = wlr_seat_get_keyboard(seat);

	//获取当前按键的mask,比如alt+super或者alt+ctrl
	mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	if (event->orientation == WLR_AXIS_ORIENTATION_VERTICAL)
		adir = event->delta > 0 ? AxisDown : AxisUp;
	else
		adir = event->delta > 0 ? AxisRight : AxisLeft;

	//处理滚轮事件绑定的函数
	for (a = axes; a < END(axes); a++) {
		if (CLEANMASK(mods) == CLEANMASK(a->mod) && //按键一致
				adir == a->dir && a->func) { //滚轮方向判断一致且处理函数存在
			a->func(&a->arg);
			return; //如果成功匹配就不把这个滚轮事件传送给客户端了
		}
	}
	/* TODO: allow usage of scroll whell for mousebindings, it can be implemented
	 * checking the event's orientation and the delta of the event */
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat, //滚轮事件发送给客户端也就是窗口
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source);


}

void //鼠标按键事件
buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *keyboard;
	uint32_t mods;
	Client *c;
	const Button *b;
	// IDLE_NOTIFY_ACTIVITY;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		cursor_mode = CurPressed;
		selmon = xytomon(cursor->x, cursor->y);
		if (locked)
			break;

		/* Change focus if the button was _pressed_ over a client */
		xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
		if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
			focusclient(c, 1);

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		for (b = buttons; b < END(buttons); b++) {
			if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
					event->button == b->button && b->func && (selmon->isoverview == 1 || b->button == BTN_MIDDLE ) && c) {
				b->func(&b->arg);
				return;
			} else if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
					event->button == b->button && b->func && CLEANMASK(b->mod) != 0 ) {
				b->func(&b->arg);
				return;

			}
		}
		break;
	case WLR_BUTTON_RELEASED:
		/* If you released any buttons, we exit interactive move/resize mode. */
		if (!locked && cursor_mode != CurNormal && cursor_mode != CurPressed) {
			cursor_mode = CurNormal;
			/* Clear the pointer focus, this way if the cursor is over a surface
			 * we will send an enter event after which the client will provide us
			 * a cursor surface */
			wlr_seat_pointer_clear_focus(seat);
			motionnotify(0, NULL, 0, 0, 0, 0);
			/* Drop the window off on its new monitor */
			selmon = xytomon(cursor->x, cursor->y);
			setmon(grabc, selmon, 0);
			grabc = NULL;
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat,
			event->time_msec, event->button, event->state);
}

void //0.5
chvt(const Arg *arg)
{
	wlr_session_change_vt(session, arg->ui);
}

void // 0.5
checkidleinhibitor(struct wlr_surface *exclude)
{
	int inhibited = 0, unused_lx, unused_ly;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link) {
		struct wlr_surface *surface = wlr_surface_get_root_surface(inhibitor->surface);
		struct wlr_scene_tree *tree = surface->data;
		if (exclude != surface && (bypass_surface_visibility || (!tree
				|| wlr_scene_node_coords(&tree->node, &unused_lx, &unused_ly)))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void //0.5
setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event = data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
				wlr_cursor_shape_v1_name(event->shape));
}


void //17
cleanup(void)
{
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
#endif
	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_backend_destroy(backend);
	wlr_scene_node_destroy(&scene->tree.node);
	wlr_renderer_destroy(drw);
	wlr_allocator_destroy(alloc);
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_cursor_destroy(cursor);
	wlr_output_layout_destroy(output_layout);
	wlr_seat_destroy(seat);
	wl_display_destroy(dpy);
}

void //17
cleanupkeyboard(struct wl_listener *listener, void *data)
{
	Keyboard *kb = wl_container_of(listener, kb, destroy);

	wl_event_source_remove(kb->key_repeat_source);
	wl_list_remove(&kb->link);
	wl_list_remove(&kb->modifiers.link);
	wl_list_remove(&kb->key.link);
	wl_list_remove(&kb->destroy.link);
	free(kb);
}

void //0.5
cleanupmon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	size_t i;

	/* m->layers[i] are intentionally not unlinked */
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);

	closemon(m);
	wlr_scene_node_destroy(&m->fullscreen_bg->node);
	free(m);
}


void
closemon(Monitor *m) //0.5
{
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c;
	int i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do /* don't switch to disabled mons */
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	}

	wl_list_for_each(c, &clients, link) {
		if (c->isfloating && c->geom.x > m->m.width)
			resize(c, (struct wlr_box){.x = c->geom.x - m->w.width, .y = c->geom.y,
					.width = c->geom.width, .height = c->geom.height}, 0);
		if (c->mon == m) {
			setmon(c, selmon, c->tags);
			reset_foreign_tolevel(c);
		}
	}
	focusclient(focustop(selmon), 1);
	printstatus();
} 

void //0.5
commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->current.layer]];

	if (layer_surface->current.committed == 0 && l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (scene_layer != l->scene->node.parent) {
		wlr_scene_node_reparent(&l->scene->node, scene_layer);
		wl_list_remove(&l->link);
		wl_list_insert(&l->mon->layers[layer_surface->current.layer], &l->link);
		wlr_scene_node_reparent(&l->popups->node, (layer_surface->current.layer
				< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer));
	}

	arrangelayers(l->mon);
}

void //0.5
commitnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, commit);

	if (client_surface(c)->mapped && c->mon)
		resize(c, c->geom, (c->isfloating && !c->isfullscreen));

	/* mark a pending resize as completed */
	if (c->resize && c->resize <= c->surface.xdg->current.configure_serial)
		c->resize = 0;
}

void //0.5
destroydecoration(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
	motionnotify(0, NULL, 0, 0, 0, 0);
}


void //0.5
createdecoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode, requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration, destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}


void //0.5
createidleinhibitor(struct wl_listener *listener, void *data)
{
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void //17
createkeyboard(struct wlr_keyboard *keyboard)
{
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	Keyboard *kb = keyboard->data = ecalloc(1, sizeof(*kb));
	kb->wlr_keyboard = keyboard;

	/* Prepare an XKB keymap and assign it to the keyboard. */
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names(context, &xkb_rules,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(keyboard, repeat_rate, repeat_delay);

	if(numlockon == 1){
		uint32_t leds = 0 ;
		xkb_mod_mask_t locked_mods = 0;
		leds =  leds | WLR_LED_NUM_LOCK;
		//获取numlock所在的位置
		xkb_mod_index_t mod_index = xkb_map_mod_get_index(keymap,
				XKB_MOD_NAME_NUM);
		if (mod_index != XKB_MOD_INVALID) {
			locked_mods |= (uint32_t)1 << mod_index; //将该位置设置为1,默认为0表示锁住小键盘
		}
		//设置numlock为on
		xkb_state_update_mask(keyboard->xkb_state, 0, 0,
		locked_mods, 0, 0, 0); 
		wlr_keyboard_led_update(keyboard,leds); //将表示numlockon的灯打开
	}

	/* Here we set up listeners for keyboard events. */
	LISTEN(&keyboard->events.modifiers, &kb->modifiers, keypressmod);
	LISTEN(&keyboard->events.key, &kb->key, keypress);
	LISTEN(&keyboard->base.events.destroy, &kb->destroy, cleanupkeyboard);

	wlr_seat_set_keyboard(seat, keyboard);

	kb->key_repeat_source = wl_event_loop_add_timer(
			wl_display_get_event_loop(dpy), keyrepeat, kb);

	/* And add the keyboard to our list of keyboards */
	wl_list_insert(&keyboards, &kb->link);
}

void //0.5
createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->pending.layer]];
	struct wlr_layer_surface_v1_state old_state;

	if (!layer_surface->output
			&& !(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.commit, &l->surface_commit, commitlayersurfacenotify);
	LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);
	LISTEN(&layer_surface->events.destroy, &l->destroy, destroylayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer = wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(layer_surface->current.layer
			< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer],&l->link);
	wlr_surface_send_enter(surface, layer_surface->output);

	/* Temporarily set the layer's current state to pending
	 * so that we can easily arrange it
	 */
	old_state = layer_surface->current;
	layer_surface->current = layer_surface->pending;
	l->mapped = 1;
	arrangelayers(l->mon);
	layer_surface->current = old_state;
}

void //0.5
createlocksurface(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data
			= wlr_scene_subsurface_tree_create(lock->scene, lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width, m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface, destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface, wlr_seat_get_keyboard(seat));
}

void //17  need fix 0.5
createmon(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct wlr_output *wlr_output = data;
	const MonitorRule *r;
	size_t i;
	Monitor *m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

    wl_list_init(&m->dwl_ipc_outputs);
	wlr_output_init_render(wlr_output, alloc, drw);

	/* Initialize monitor state using configured rules */
	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	m->gappih = gappih;
	m->gappiv = gappiv;
	m->gappoh = gappoh;
	m->gappov = gappov;
	m->isoverview = 0;
	m->sel = NULL;
	m->is_in_hotarea = 0;
	m->tagset[0] = m->tagset[1] = 1;
	float scale = 1;
	m->mfact = 0.5;
	m->nmaster = 1;
	enum wl_output_transform rr = WL_OUTPUT_TRANSFORM_NORMAL;

	if(LENGTH(layouts) > 1){
		m->lt[0] = &layouts[0]; //默认就有两个布局
		m->lt[1] = &layouts[1];
	}else{
		m->lt[0] = m->lt[1] = &layouts[0];
	}	
	for (r = monrules; r < END(monrules); r++) {
		if (!r->name || strstr(wlr_output->name, r->name)) {
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			m->m.x = r->x;
			m->m.y = r->y;
			if(r->lt)
				m->lt[0] = m->lt[1] = r->lt;
			scale = r->scale;
			rr = r->rr;
			break;
		}
	}

	wlr_output_set_scale(wlr_output,scale);
	wlr_xcursor_manager_load(cursor_mgr, scale);
	wlr_output_set_transform(wlr_output, rr);
	/* The mode is a tuple of (width, height, refresh rate), and each
	 * monitor supports only a specific set of modes. We just pick the
	 * monitor's preferred mode; a more sophisticated compositor would let
	 * the user configure it. */
	wlr_output_set_mode(wlr_output, wlr_output_preferred_mode(wlr_output));

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state, requestmonstate);

	wlr_output_enable(wlr_output, 1);
	if (!wlr_output_commit(wlr_output))
		return;

	/* Try to enable adaptive sync, note that not all monitors support it.
	 * wlr_output_commit() will deactivate it in case it cannot be enabled */
	wlr_output_enable_adaptive_sync(wlr_output, 1);
	wlr_output_commit(wlr_output);

	wl_list_insert(&mons, &m->link);
	printstatus();

	m->pertag = calloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= LENGTH(tags); i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;
	}


	/* The xdg-protocol specifies:
	 *
	 * If the fullscreened surface is not opaque, the compositor must make
	 * sure that other screen content not part of the same surface tree (made
	 * up of subsurfaces, popups or similarly coupled surfaces) are not
	 * visible below the fullscreened surface.
	 *
	 */
	/* updatemons() will resize and set correct position */
	m->fullscreen_bg = wlr_scene_rect_create(layers[LyrFS], 0, 0, fullscreen_bg);
	wlr_scene_node_set_enabled(&m->fullscreen_bg->node, 0);

	/* Adds this to the output layout in the order it was configured in.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x < 0 || m->m.y < 0)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x, m->m.y);
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));
}



void //fix for 0.5
createnotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_surface *xdg_surface = data;
	Client *c = NULL;
	LayerSurface *l = NULL;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_popup *popup = xdg_surface->popup;
		struct wlr_box box;
		if (toplevel_from_wlr_surface(popup->base->surface, &c, &l) < 0)
			return;
		popup->base->surface->data = wlr_scene_xdg_surface_create(
				popup->parent->data, popup->base);
		if ((l && !l->mon) || (c && !c->mon))
			return;
		box = l ? l->mon->m : c->mon->w;
		box.x -= (l ? l->geom.x : c->geom.x);
		box.y -= (l ? l->geom.y : c->geom.y);
		wlr_xdg_popup_unconstrain_from_box(popup, &box);
		return;
	} else if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
		return;

	/* Allocate a Client for this surface */
	c = xdg_surface->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = xdg_surface;
	c->bw = borderpx;

	wlr_xdg_toplevel_set_wm_capabilities(xdg_surface->toplevel,
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

	LISTEN(&xdg_surface->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&xdg_surface->surface->events.map, &c->map, mapnotify);
	LISTEN(&xdg_surface->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&xdg_surface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xdg_surface->toplevel->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xdg_surface->toplevel->events.request_fullscreen, &c->fullscreen,
			fullscreennotify);
	LISTEN(&xdg_surface->toplevel->events.request_maximize, &c->maximize,
			maximizenotify);
	LISTEN(&xdg_surface->toplevel->events.request_minimize, &c->minimize,
			minimizenotify);
}

void // 0.5
createpointer(struct wlr_pointer *pointer)
{
	struct libinput_device *device;
	if (wlr_input_device_is_libinput(&pointer->base)
			&& (device = wlr_libinput_get_device_handle(&pointer->base))) {

		if (libinput_device_config_tap_get_finger_count(device)) {
			libinput_device_config_tap_set_enabled(device, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
			libinput_device_config_tap_set_button_map(device, button_map);
		}

		if (libinput_device_config_scroll_has_natural_scroll(device))
			libinput_device_config_scroll_set_natural_scroll_enabled(device, natural_scrolling);

		if (libinput_device_config_dwt_is_available(device))
			libinput_device_config_dwt_set_enabled(device, disable_while_typing);

		if (libinput_device_config_left_handed_is_available(device))
			libinput_device_config_left_handed_set(device, left_handed);

		if (libinput_device_config_middle_emulation_is_available(device))
			libinput_device_config_middle_emulation_set_enabled(device, middle_button_emulation);

		if (libinput_device_config_scroll_get_methods(device) != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method (device, scroll_method);

		if (libinput_device_config_click_get_methods(device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method (device, click_method);

		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(device, send_events_mode);

		if (libinput_device_config_accel_is_available(device)) {
			libinput_device_config_accel_set_profile(device, accel_profile);
			libinput_device_config_accel_set_speed(device, accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void // 0.5
createpointerconstraint(struct wl_listener *listener, void *data)
{
	PointerConstraint *pointer_constraint = ecalloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
	LISTEN(&pointer_constraint->constraint->events.destroy,
			&pointer_constraint->destroy, destroypointerconstraint);
}

void // 0.5
cursorconstrain(struct wlr_pointer_constraint_v1 *constraint)
{
	if (active_constraint == constraint)
		return;

	if (active_constraint)
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);

	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}


void //0.5
cursorframe(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void // 0.5
cursorwarptohint(void)
{
	Client *c = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
	/* TODO: wlroots 0.18: https://gitlab.freedesktop.org/wlroots/wlroots/-/merge_requests/4478 */
	if (c && (active_constraint->current.committed & WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT )) {
		wlr_cursor_warp(cursor, NULL, sx + c->geom.x + c->bw, sy + c->geom.y + c->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}

void
defaultgaps(const Arg *arg)
{
	setgaps(gappoh, gappov, gappih, gappiv);
}



void //0.5
destroydragicon(struct wl_listener *listener, void *data)
{
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void //17
destroyidleinhibitor(struct wl_listener *listener, void *data)
{
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
}

void //0.5
destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->scene->node);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void //0.5
destroylock(SessionLock *lock, int unlock)
{
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	focusclient(focustop(selmon), 0);
	motionnotify(0, NULL, 0, 0, 0, 0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void //0.5
destroylocksurface(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface, *lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface)
		return;

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface = wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface, wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		focusclient(focustop(selmon), 1);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}


void //0.5
destroynotify(struct wl_listener *listener, void *data)
{
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
	wl_list_remove(&c->maximize.link);
	wl_list_remove(&c->minimize.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	free(c);
}

void // 0.5
destroypointerconstraint(struct wl_listener *listener, void *data)
{
	PointerConstraint *pointer_constraint = wl_container_of(listener, pointer_constraint, destroy);

	if (active_constraint == pointer_constraint->constraint) {
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
}

void //0.5
destroysessionlock(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
	motionnotify(0, NULL, 0, 0, 0, 0);
}


void //0.5
destroysessionmgr(struct wl_listener *listener, void *data)
{
	wl_list_remove(&lock_listener.link);
	wl_list_remove(&listener->link);
}

void //17
associatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.commit, &c->commit, commitnotify);
	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void //17
dissociatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->commit.link);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}


Monitor * //0.5
dirtomon(enum wlr_direction dir)
{
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(output_layout,
			dir, selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(output_layout,
			dir ^ (WLR_DIRECTION_LEFT|WLR_DIRECTION_RIGHT),
			selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}



void dwl_ipc_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *manager_resource = wl_resource_create(client, &zdwl_ipc_manager_v2_interface, version, id);
    if (!manager_resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(manager_resource, &dwl_manager_implementation, NULL, dwl_ipc_manager_destroy);

    zdwl_ipc_manager_v2_send_tags(manager_resource, LENGTH(tags));

    for (int i = 0; i < LENGTH(layouts); i++)
        zdwl_ipc_manager_v2_send_layout(manager_resource, layouts[i].symbol);
}

void dwl_ipc_manager_destroy(struct wl_resource *resource) {
    /* No state to destroy */
}

void dwl_ipc_manager_get_output(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *output) {
    DwlIpcOutput *ipc_output;
    Monitor *monitor = wlr_output_from_resource(output)->data;
    struct wl_resource *output_resource = wl_resource_create(client, &zdwl_ipc_output_v2_interface, wl_resource_get_version(resource), id);
    if (!output_resource)
        return;

    ipc_output = ecalloc(1, sizeof(*ipc_output));
    ipc_output->resource = output_resource;
    ipc_output->monitor = monitor;
    wl_resource_set_implementation(output_resource, &dwl_output_implementation, ipc_output, dwl_ipc_output_destroy);
    wl_list_insert(&monitor->dwl_ipc_outputs, &ipc_output->link);
    dwl_ipc_output_printstatus_to(ipc_output);
}

void dwl_ipc_manager_release(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}

// 在外部ipc客户端结束的时候会发出销毁请求,比如kill掉waybar,就会销毁waybar绑定的ipc_output
static void dwl_ipc_output_destroy(struct wl_resource *resource) {
    DwlIpcOutput *ipc_output = wl_resource_get_user_data(resource);
    wl_list_remove(&ipc_output->link);
    free(ipc_output);
}

void dwl_ipc_output_printstatus(Monitor *monitor) {
    DwlIpcOutput *ipc_output;
    wl_list_for_each(ipc_output, &monitor->dwl_ipc_outputs, link)
    	dwl_ipc_output_printstatus_to(ipc_output);
}

void dwl_ipc_output_printstatus_to(DwlIpcOutput *ipc_output) {
    Monitor *monitor = ipc_output->monitor;
    Client *c, *focused;
    int tagmask, state, numclients, focused_client, tag;
    const char *title, *appid,*symbol;
    focused = focustop(monitor);
    zdwl_ipc_output_v2_send_active(ipc_output->resource, monitor == selmon);

	if ((monitor->tagset[monitor->seltags] & TAGMASK) == TAGMASK) {
		state = 0;
		state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;
    	zdwl_ipc_output_v2_send_tag(ipc_output->resource, 888, state, 1, 1);		
	} else {
    	for ( tag = 0 ; tag < LENGTH(tags); tag++) {
    	    numclients = state = focused_client = 0;
    	    tagmask = 1 << tag;
    	    if ((tagmask & monitor->tagset[monitor->seltags]) != 0)
    	        state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_ACTIVE;

    	    wl_list_for_each(c, &clients, link) {
    	        if (c->mon != monitor)
    	            continue;
    	        if (!(c->tags & tagmask))
    	            continue;
    	        if (c == focused)
    	            focused_client = 1;
    	        if (c->isurgent)
    	            state |= ZDWL_IPC_OUTPUT_V2_TAG_STATE_URGENT;

    	        numclients++;
    	    }
    	    zdwl_ipc_output_v2_send_tag(ipc_output->resource, tag, state, numclients, focused_client);
    	}
	}


    title = focused ? client_get_title(focused) : "";
    appid = focused ? client_get_appid(focused) : "";
	symbol = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt]->symbol;

    zdwl_ipc_output_v2_send_layout(ipc_output->resource, monitor->lt[monitor->sellt] - layouts);
    zdwl_ipc_output_v2_send_title(ipc_output->resource, title ? title : broken); 
    zdwl_ipc_output_v2_send_appid(ipc_output->resource, appid ? appid : broken);
    zdwl_ipc_output_v2_send_layout_symbol(ipc_output->resource, symbol);
    zdwl_ipc_output_v2_send_frame(ipc_output->resource);
}

void dwl_ipc_output_set_client_tags(struct wl_client *client, struct wl_resource *resource, uint32_t and_tags, uint32_t xor_tags) {
    DwlIpcOutput *ipc_output;
    Monitor *monitor;
    Client *selected_client;
    unsigned int newtags = 0;

    ipc_output = wl_resource_get_user_data(resource);
    if (!ipc_output)
        return;

    monitor = ipc_output->monitor;
    selected_client = focustop(monitor);
    if (!selected_client)
        return;

    newtags = (selected_client->tags & and_tags) ^ xor_tags;
    if (!newtags)
        return;

    selected_client->tags = newtags;
    focusclient(focustop(selmon), 1);
    arrange(selmon);
    printstatus();
}

void dwl_ipc_output_set_layout(struct wl_client *client, struct wl_resource *resource, uint32_t index) {
    DwlIpcOutput *ipc_output;
    Monitor *monitor;

    ipc_output = wl_resource_get_user_data(resource);
    if (!ipc_output)
        return;

    monitor = ipc_output->monitor;
    if (index >= LENGTH(layouts))
        return;
    if (index != monitor->lt[monitor->sellt] - layouts)
        monitor->sellt ^= 1;

    monitor->lt[monitor->sellt] = &layouts[index];
    arrange(monitor);
    printstatus();
}

void dwl_ipc_output_set_tags(struct wl_client *client, struct wl_resource *resource, uint32_t tagmask, uint32_t toggle_tagset) {
    DwlIpcOutput *ipc_output;
    Monitor *monitor;
    unsigned int newtags = tagmask & TAGMASK;

    ipc_output = wl_resource_get_user_data(resource);
    if (!ipc_output)
        return;
    monitor = ipc_output->monitor;

    if (!newtags || newtags == monitor->tagset[monitor->seltags])
        return;
    if (toggle_tagset)
        monitor->seltags ^= 1;

    monitor->tagset[monitor->seltags] = newtags;
    focusclient(focustop(monitor), 1);
    arrange(monitor);
    printstatus();
}

void dwl_ipc_output_release(struct wl_client *client, struct wl_resource *resource) {
    wl_resource_destroy(resource);
}


void
focusclient(Client *c, int lift)
{
	struct wlr_surface *old = seat->keyboard_state.focused_surface;
	if (locked)
		return;

    if (c && c->iskilling)
		return;

	/* Raise client in stacking order if requested */
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node); //将视图提升到顶层

	if (c && client_surface(c) == old)
		return;

	if (c && c->mon && c->mon != selmon) {
		selmon = c->mon;
	}

	if(selmon && selmon->sel && selmon->sel->foreign_toplevel) {
		wlr_foreign_toplevel_handle_v1_set_activated(selmon->sel->foreign_toplevel,false);
	}
	if(selmon)
		selmon->sel  = c;
	if(c && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,true);

	/* Put the new client atop the focus stack and select its monitor */
	if (c && !client_is_unmanaged(c)) {
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);
		selmon = c->mon;
		c->isurgent = 0;
		client_restack_surface(c);
		setborder_color(c);
		/* Don't change border color if there is an exclusive focus or we are
		 * handling a drag operation */
	}

	/* Deactivate old client if focus is changing */
	if (old && (!c || client_surface(c) != old)) {
		/* If an overlay is focused, don't focus or activate the client,
		 * but only update its position in fstack to render its border with focuscolor
		 * and focus it after the overlay is closed. */
		Client *w = NULL;
		LayerSurface *l = NULL;
		int type = toplevel_from_wlr_surface(old, &w, &l);
		if (type == LayerShell && l->scene->node.enabled
				&& l->layer_surface->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
			return;
		} else if (w && w == exclusive_focus && client_wants_focus(w)) {
			return;
		/* Don't deactivate old client if the new one wants focus, as this causes issues with winecfg
		 * and probably other clients */
		} else if (w && !client_is_unmanaged(w) && (!c || !client_wants_focus(c))) {
				setborder_color(w);

			client_activate_surface(old, 0);
		}
	}
	printstatus();

	if (!c) {
		/* With no client, all we have left is to clear focus */
		if(selmon && selmon->sel)
			selmon->sel  = NULL; //这个很关键,因为很多地方用到当前窗口做计算,不重置成NULL就会到处有野指针
		#ifdef IM
		    dwl_input_method_relay_set_focus(input_relay, NULL);
		#endif
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	}

	/* Change cursor surface */
	motionnotify(0, NULL, 0, 0, 0, 0);

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

#ifdef IM
		struct wlr_keyboard *keyboard;
		keyboard = wlr_seat_get_keyboard(seat);
		uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		if (mods == 0)
			dwl_input_method_relay_set_focus(input_relay, client_surface(c));
#endif
	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);
}

void // 0.5
focusmon(const Arg *arg)
{
	int i = 0, nmons = wl_list_length(&mons);
	if (nmons) {
		do /* don't switch to disabled mons */
			selmon = dirtomon(arg->i);
		while (!selmon->wlr_output->enabled && i++ < nmons);
		if (!selmon->wlr_output->enabled)
			selmon = NULL;
	}
	warp_cursor_to_selmon(selmon);
	focusclient(focustop(selmon), 1);
}

void //17
focusstack(const Arg *arg)
{
	/* Focus the next or previous client (in tiling order) on selmon */
	Client *c, *sel = focustop(selmon);
	if (!sel || sel->isfullscreen)
		return;
	if (arg->i > 0) {
		wl_list_for_each(c, &sel->link, link) {
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	} else {
		wl_list_for_each_reverse(c, &sel->link, link) {
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	}
	/* If only one client is visible on selmon, then c == sel */
	focusclient(c, 1);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client * //0.5
focustop(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &fstack, flink) {
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

void
fullscreennotify(struct wl_listener *listener, void *data)
{
	// Client *c = wl_container_of(listener, c, fullscreen);
	// setfullscreen(c, client_wants_fullscreen(c));
	Client *c = selmon->sel;
	if(!c){
		return;  //没有聚焦的窗口就什么也不操作
	}
	if(c->isrealfullscreen){
		setrealfullscreen(c,0); //自带的全屏函数容易黑屏有bug,换这个就没有问题
	}else {
		setrealfullscreen(c,1);
	}
}

void
incnmaster(const Arg *arg)
{
	if (!arg || !selmon)
		return;
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

void
incgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov + arg->i,
		selmon->gappih + arg->i,
		selmon->gappiv + arg->i
	);
}

void
incigaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih + arg->i,
		selmon->gappiv + arg->i
	);
}

void
incihgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih + arg->i,
		selmon->gappiv
	);
}

void
incivgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov,
		selmon->gappih,
		selmon->gappiv + arg->i
	);
}

void
incogaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov + arg->i,
		selmon->gappih,
		selmon->gappiv
	);
}

void
incohgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh + arg->i,
		selmon->gappov,
		selmon->gappih,
		selmon->gappiv
	);
}

void
requestmonstate(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}


void
incovgaps(const Arg *arg)
{
	setgaps(
		selmon->gappoh,
		selmon->gappov + arg->i,
		selmon->gappih,
		selmon->gappiv
	);
}

void //17
inputdevice(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	/* TODO do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&keyboards))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

int
keyrepeat(void *data)
{
	Keyboard *kb = data;
	int i;
	if (!kb->nsyms || kb->wlr_keyboard->repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(kb->key_repeat_source,
			1000 / kb->wlr_keyboard->repeat_info.rate);

	for (i = 0; i < kb->nsyms; i++)
		keybinding(kb->mods, kb->keysyms[i]);

	return 0;
}


int //17
keybinding(uint32_t mods, xkb_keysym_t sym)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	int handled = 0;
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod) &&
				sym == k->keysym && k->func) {
			k->func(&k->arg);
			handled = 1;
		}
	}
	return handled;
}

void //17
keypress(struct wl_listener *listener, void *data)
{
	int i;
	/* This event is raised when a key is pressed or released. */
	Keyboard *kb = wl_container_of(listener, kb, key);
	struct wlr_keyboard_key_event *event = data;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
			kb->wlr_keyboard->xkb_state, keycode, &syms);

	int handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(kb->wlr_keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
#ifdef IM
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_RELEASED && (keycode == 133 ||keycode == 37 || keycode == 64 || keycode == 50 || keycode == 134 || keycode == 105 || keycode == 108 || keycode == 62)  && selmon->sel) {
		dwl_input_method_relay_set_focus(input_relay, client_surface(selmon->sel));
	}	
#endif		

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED)
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i]) || handled;

	if (handled && kb->wlr_keyboard->repeat_info.delay > 0) {
		kb->mods = mods;
		kb->keysyms = syms;
		kb->nsyms = nsyms;
		wl_event_source_timer_update(kb->key_repeat_source,
				kb->wlr_keyboard->repeat_info.delay);
	} else {
		kb->nsyms = 0;
		wl_event_source_timer_update(kb->key_repeat_source, 0);
	}

	if (handled)
		return;


#ifdef IM
	  /* if there is a keyboard grab, we send the key there */
	struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(kb);
	if (kb_grab) {
		wlr_input_method_keyboard_grab_v2_set_keyboard(kb_grab,kb->wlr_keyboard);
		wlr_input_method_keyboard_grab_v2_send_key(kb_grab,event->time_msec, event->keycode, event->state);
		wlr_log(WLR_DEBUG, "keypress send to IM:%u mods %u state %u",event->keycode, mods,event->state);
		return;
	}
#endif

	/* Pass unhandled keycodes along to the client. */
	wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
	wlr_seat_keyboard_notify_key(seat, event->time_msec,
		event->keycode, event->state);
}


void //17
keypressmod(struct wl_listener *listener, void *data)
{
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	Keyboard *kb = wl_container_of(listener, kb, modifiers);
#ifdef IM
        struct wlr_input_method_keyboard_grab_v2 *kb_grab = keyboard_get_im_grab(kb);
	if (kb_grab) {
		wlr_input_method_keyboard_grab_v2_send_modifiers(kb_grab,
				&kb->wlr_keyboard->modifiers);
		wlr_log(WLR_DEBUG, "keypressmod send to IM");
		return;
	}
#endif
	/*
	 * A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same seat. You can swap out the underlying wlr_keyboard like this and
	 * wlr_seat handles this transparently.
	 */
	wlr_seat_set_keyboard(seat, kb->wlr_keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(seat,
		&kb->wlr_keyboard->modifiers);
}


void
killclient(const Arg *arg)
{	
	Client *c;
	c = selmon->sel;
	if (c) {
		c->iskilling = 1;
		selmon->sel = NULL;
		client_send_close(c);
	}
		
}

void //0.5
locksession(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	wlr_scene_node_set_enabled(&locked_bg->node, 1);
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface, createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy, destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

void //0.5
maplayersurfacenotify(struct wl_listener *listener, void *data)
{
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void //old fix to 0.5
mapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *p = NULL;
	Client *c = wl_container_of(listener, c, map);
	int i;

	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data = wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface = c->type == XDGShell
			? wlr_scene_xdg_surface_create(c->scene, c->surface.xdg)
			: wlr_scene_subsurface_tree_create(c->scene, client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	/* Handle unmanaged clients first so we can return prior create borders */
	if (client_is_unmanaged(c)) {
		/* Unmanaged clients always are floating */
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		wlr_scene_node_set_position(&c->scene->node, c->geom.x + borderpx,
				c->geom.y + borderpx);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		return;
	}

	for (i = 0; i < 4; i++) {
		c->border[i] = wlr_scene_rect_create(c->scene, 0, 0,
				c->isurgent ? urgentcolor : bordercolor);
		c->border[i]->node.data = c;
	}

	/* Initialize client geometry with room for border */
	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;
	c->isfakefullscreen = 0;
	c->isrealfullscreen = 0;	
	c->istiled = 0;
	c->ignore_clear_fullscreen = 0;
	c->iskilling = 0;
	//nop
	if (new_is_master)
		// tile at the top
		wl_list_insert(&clients, &c->link); //新窗口是master,头部入栈
	else
		wl_list_insert(clients.prev, &c->link); //尾部入栈
	wl_list_insert(&fstack, &c->flink);

	/* Set initial monitor, tags, floating status, and focus:
	 * we always consider floating, clients that have parent and thus
	 * we set the same tags and monitor than its parent, if not
	 * try to apply rules for them */
	if ((p = client_get_parent(c))) {
		c->isfloating = 1;
		setmon(c, p->mon, p->tags);
	} else {
		applyrules(c);
	}

	//创建外部顶层窗口的句柄,每一个顶层窗口都有一个
	c->foreign_toplevel = wlr_foreign_toplevel_handle_v1_create(foreign_toplevel_manager);

	//监听来自外部对于窗口的事件请求
	if(c->foreign_toplevel){
		LISTEN(&(c->foreign_toplevel->events.request_activate),
				&c->foreign_activate_request,handle_foreign_activate_request);
		LISTEN(&(c->foreign_toplevel->events.request_fullscreen),
				&c->foreign_fullscreen_request,handle_foreign_fullscreen_request);
		LISTEN(&(c->foreign_toplevel->events.request_close),
				&c->foreign_close_request,handle_foreign_close_request);
		LISTEN(&(c->foreign_toplevel->events.destroy),
				&c->foreign_destroy,handle_foreign_destroy);
		//设置外部顶层句柄的id为应用的id
		const char *appid;
    	appid = client_get_appid(c) ;
		if(appid)
			wlr_foreign_toplevel_handle_v1_set_app_id(c->foreign_toplevel,appid);
		//设置外部顶层句柄的title为应用的title
		const char *title;
    	title = client_get_title(c) ;
		if(title)
			wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel,title);
		//设置外部顶层句柄的显示监视器为当前监视器
		wlr_foreign_toplevel_handle_v1_output_enter(
				c->foreign_toplevel, selmon->wlr_output);
	}

	if(selmon->sel && selmon->sel->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(selmon->sel->foreign_toplevel,false);
	selmon->sel  = c;
	if(c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,true);

	printstatus();

}

void //0.5
maximizenotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	togglefakefullscreen(&(Arg){0});
}

void set_minized(Client *c) {
  	if (c->isglobal) {
  	  c->isglobal = 0;
  	  selmon->sel->tags = selmon->tagset[selmon->seltags];
  	}
	c->is_scratchpad_show = 0;
	c->oldtags = c->mon->sel->tags;
	c->tags = 0;
	c->isminied = 1;
	c->is_in_scratchpad = 1;
	c->is_scratchpad_show = 0;
	focusclient(focustop(selmon), 1);
	arrange(c->mon);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,false);
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,true);
	wl_list_remove(&c->link);            //从原来位置移除
	wl_list_insert(clients.prev, &c->link); //插入尾部

}

void //0.5
minimizenotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	// Client *c = wl_container_of(listener, c, maximize);
	// if (wl_resource_get_version(c->surface.xdg->toplevel->resource)
	// 		< XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
	// 	wlr_xdg_surface_schedule_configure(c->surface.xdg);
	// togglefakefullscreen(&(Arg){0});
	if(selmon->sel) {
		set_minized(selmon->sel);
	}
}



void //17
monocle(Monitor *m)
{
	Client *c;
	int n = 0;

	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		resize(c, m->w, 0);
		n++;
	}
	if (n)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

void //0.5
motionabsolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	double lx, ly, dx, dy;

	if (!event->time_msec) /* this is 0 with virtual pointers */
		wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);

	wlr_cursor_absolute_to_layout_coords(cursor, &event->pointer->base, event->x, event->y, &lx, &ly);
	dx = lx - cursor->x;
	dy = ly - cursor->y;
	motionnotify(event->time_msec, &event->pointer->base, dx, dy, dx, dy);
}

void //fix for 0.5
motionnotify(uint32_t time, struct wlr_input_device *device, double dx, double dy,
		double dx_unaccel, double dy_unaccel)
{
	double sx = 0, sy = 0, sx_confined, sy_confined;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;
	struct wlr_pointer_constraint_v1 *constraint;

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag
			&& surface != seat->pointer_state.focused_surface
			&& toplevel_from_wlr_surface(seat->pointer_state.focused_surface, &w, &l) >= 0) {
		c = w;
		surface = seat->pointer_state.focused_surface;
		sx = cursor->x - (l ? l->geom.x : w->geom.x);
		sy = cursor->y - (l ? l->geom.y : w->geom.y);
	}

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_relative_pointer_manager_v1_send_relative_motion(
			relative_pointer_mgr, seat, (uint64_t)time * 1000,
			dx, dy, dx_unaccel, dy_unaccel);

		wl_list_for_each(constraint, &pointer_constraints->constraints, link)
			cursorconstrain(constraint);

		if (active_constraint && cursor_mode != CurResize && cursor_mode != CurMove) {
			toplevel_from_wlr_surface(active_constraint->surface, &c, NULL);
			if (c && active_constraint->surface == seat->pointer_state.focused_surface) {
				sx = cursor->x - c->geom.x - c->bw;
				sy = cursor->y - c->geom.y - c->bw;
				if (wlr_region_confine(&active_constraint->region, sx, sy,
						sx + dx, sy + dy, &sx_confined, &sy_confined)) {
					dx = sx_confined - sx;
					dy = sy_confined - sy;
				}

				if (active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED)
					return;
			}
		}

		wlr_cursor_move(cursor, device, dx, dy);
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
		}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, cursor->x, cursor->y);

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		resize(grabc, (struct wlr_box){.x = cursor->x - grabcx, .y = cursor->y - grabcy,
			.width = grabc->geom.width, .height = grabc->geom.height}, 1);
		return;
	} else if (cursor_mode == CurResize) {
		resize(grabc, (struct wlr_box){.x = grabc->geom.x, .y = grabc->geom.y,
			.width = cursor->x - grabc->geom.x, .height = cursor->y - grabc->geom.y}, 1);
		return;
	}

	/* If there's no client surface under the cursor, set the cursor image to a
	 * default. This is what makes the cursor image appear when you move it
	 * off of a client or over its border. */
	if (!surface && !seat->drag)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");

	pointerfocus(c, surface, sx, sy, time);
}


void // fix for 0.5 光标相对位置移动事件处理
motionrelative(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */


	// //处理一些事件,比如窗口聚焦,图层聚焦通知到客户端
	// motionnotify(event->time_msec);  
	// //扩展事件通知,没有这个鼠标移动的时候滑轮将无法使用
	// wlr_relative_pointer_manager_v1_send_relative_motion(
	// 	pointer_manager,
	// 	seat, (uint64_t)(event->time_msec) * 1000,
	// 	event->delta_x, event->delta_y, 
	// 	event->unaccel_dx, event->unaccel_dy);
	// //通知光标设备移动
	// wlr_cursor_move(cursor, &event->pointer->base, event->delta_x, event->delta_y);
	motionnotify(event->time_msec, &event->pointer->base, event->delta_x, event->delta_y,
			event->unaccel_dx, event->unaccel_dy);
	//鼠标左下热区判断是否触发	
	toggle_hotarea(cursor->x,cursor->y);	
}

void //17
moveresize(const Arg *arg)
{
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen)
		return;

	/* Float the window and tell motionnotify to grab it */
	if(grabc->isfloating == 0) {
		setfloating(grabc, 1);
	} 
	
	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = cursor->x - grabc->geom.x;
		grabcy = cursor->y - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "fleur");
		break;
	case CurResize:
		/* Doesn't work for X11 output - the next absolute motion event
		 * returns the cursor to where it started */
		wlr_cursor_warp_closest(cursor, NULL,
				grabc->geom.x + grabc->geom.width,
				grabc->geom.y + grabc->geom.height);
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "bottom_right_corner");
		break;
	}
}

void //0.5
outputmgrapply(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void //0.5
outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test)
{
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link) {
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state, config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(&state,
					config_head->state.custom_mode.width,
					config_head->state.custom_mode.height,
					config_head->state.custom_mode.refresh);

		wlr_output_state_set_transform(&state, config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(&state,
				config_head->state.adaptive_sync_enabled);

apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state)
				: wlr_output_commit_state(wlr_output, &state);

		/* Don't move monitors if position wouldn't change, this to avoid
		* wlroots marking the output as manually configured.
		* wlr_output_layout_add does not like disabled outputs */
		if (!test && wlr_output->enabled && (m->m.x != config_head->state.x || m->m.y != config_head->state.y))
			wlr_output_layout_add(output_layout, wlr_output,
					config_head->state.x, config_head->state.y);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	/* TODO: use a wrapper function? */
	updatemons(NULL, NULL);
}

void //0.5
outputmgrtest(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}


void //17
pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
		uint32_t time)
{
	struct timespec now;
	int internal_call = !time;

	if (surface != seat->pointer_state.focused_surface &&
			sloppyfocus && time && c && !client_is_unmanaged(c))
		focusclient(c, 0);

	/* If surface is NULL, try use the focused client surface to set pointer foucs */
	if (time == 0 && !surface && selmon && selmon->sel) {
		surface = client_surface(selmon->sel);
	}

	/* If surface is still NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (internal_call) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);

}

void //17
printstatus(void)
{
	Monitor *m = NULL;
	Client *c;
	uint32_t occ, urg, sel;
	const char *appid, *title;

	wl_list_for_each(m, &mons, link) {
		if(!m->wlr_output->enabled) {
			continue;
		}
		occ = urg = 0;
		wl_list_for_each(c, &clients, link) {
			if (c->mon != m)
				continue;
			occ |= c->tags;
			if (c->isurgent)
				urg |= c->tags;
		}
		if ((c = focustop(m))) {
			title = client_get_title(c);
			appid = client_get_appid(c);
			printf("%s title %s\n", m->wlr_output->name, title ? title : broken);
			printf("%s appid %s\n", m->wlr_output->name, appid ? appid : broken);
			printf("%s fullscreen %u\n", m->wlr_output->name, c->isfullscreen);
			printf("%s floating %u\n", m->wlr_output->name, c->isfloating);
			sel = c->tags;
		} else {
			printf("%s title \n", m->wlr_output->name);
			printf("%s appid \n", m->wlr_output->name);
			printf("%s fullscreen \n", m->wlr_output->name);
			printf("%s floating \n", m->wlr_output->name);
			sel = 0;
		}

		printf("%s selmon %u\n", m->wlr_output->name, m == selmon);
		printf("%s tags %u %u %u %u\n", m->wlr_output->name, occ, m->tagset[m->seltags],
				sel, urg);
		printf("%s layout %s\n", m->wlr_output->name, m->ltsymbol);
		dwl_ipc_output_printstatus(m); //更新waybar上tag的状态 这里很关键
	}
	fflush(stdout);
}

void //0.5
quit(const Arg *arg)
{
	wl_display_terminate(dpy);
}

void
quitsignal(int signo)
{
	quit(NULL);
}
 
void //0.5
rendermon(struct wl_listener *listener, void *data)
{
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c;
	struct wlr_output_state pending = {0};
	struct wlr_gamma_control_v1 *gamma_control;
	struct timespec now;

	/* Render if no XDG clients have an outstanding resize and are visible on
	 * this monitor. */
	wl_list_for_each(c, &clients, link) {
		if (c->resize && !c->isfloating && client_is_rendered_on_mon(c, m) && !client_is_stopped(c))
			goto skip;
	}

	/*
	 * HACK: The "correct" way to set the gamma is to commit it together with
	 * the rest of the state in one go, but to do that we would need to rewrite
	 * wlr_scene_output_commit() in order to add the gamma to the pending
	 * state before committing, instead try to commit the gamma in one frame,
	 * and commit the rest of the state in the next one (or in the same frame if
	 * the gamma can not be committed).
	 */
	if (m->gamma_lut_changed) {
		gamma_control
				= wlr_gamma_control_manager_v1_get_control(gamma_control_mgr, m->wlr_output);
		m->gamma_lut_changed = 0;

		if (!wlr_gamma_control_v1_apply(gamma_control, &pending))
			goto commit;

		if (!wlr_output_test_state(m->wlr_output, &pending)) {
			wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
			goto commit;
		}
		wlr_output_commit_state(m->wlr_output, &pending);
		wlr_output_schedule_frame(m->wlr_output);
	} else {
commit:
		wlr_scene_output_commit(m->scene_output, NULL);
	}

skip:
	/* Let clients know a frame has been rendered */
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);
	wlr_output_state_finish(&pending);
}


void //0.5
requestdecorationmode(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	wlr_xdg_toplevel_decoration_v1_set_mode(c->decoration,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void //17
requeststartdrag(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
			event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}


void //为了兼容dwm代码封装的
resizeclient(Client *c,int x,int y,int w,int h, int interact)
{
	struct wlr_box tmp_box;
	c->prev = c->geom;
	tmp_box.x = x;
	tmp_box.y = y;
	tmp_box.width = w;
	tmp_box.height = h;
	resize(c, tmp_box, interact);
}


void setborder_color(Client *c){
	unsigned int i;
	if(!c || !c->mon)
		return;
	if (c->isurgent) {
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], urgentcolor);	
	} if (c->is_in_scratchpad && c == selmon->sel) {
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], scratchpadcolor);		
	} else if(c->isglobal && c == selmon->sel){
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], globalcolor);
	} else if(c->isfakefullscreen && c == selmon->sel){
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], fakefullscreencolor);
	} else if(c == selmon->sel) {
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], focuscolor);
	} else {
		for (i = 0; i < 4; i++)
		wlr_scene_rect_set_color(c->border[i], bordercolor);		
	}
}

void exchange_two_client(Client *c1, Client *c2) {
  if (c1 == NULL || c2 == NULL || c1->mon != c2->mon) {
    return;
  }

  struct wl_list *tmp1_prev = c1->link.prev;
  struct wl_list *tmp2_prev = c2->link.prev;
  struct wl_list *tmp1_next = c1->link.next;
  struct wl_list *tmp2_next = c2->link.next;

  // wl_list 是双向链表,其中clients是头部节点,它的下一个节点是第一个客户端的链表节点
  //最后一个客户端的链表节点的下一个节点也指向clients,但clients本身不是客户端的链表节点
  //客户端遍历从clients的下一个节点开始,到检测到客户端节点的下一个是clients结束
  
  // 当c1和c2为相邻节点时
  if (c1->link.next == &c2->link) {
    c1->link.next = c2->link.next;
	c1->link.prev = &c2->link;
    c2->link.next = &c1->link;
	c2->link.prev = tmp1_prev;
    tmp1_prev->next = &c2->link;
	tmp2_next->prev = &c1->link;
  } else if (c2->link.next == &c1->link) {
    c2->link.next = c1->link.next;
	c2->link.prev = &c1->link;
    c1->link.next = &c2->link;
	c1->link.prev = tmp2_prev;
    tmp2_prev->next = &c1->link;
	tmp1_next->prev = &c2->link;
  } else { // 不为相邻节点
    c2->link.next = tmp1_next;
	c2->link.prev = tmp1_prev;
    c1->link.next = tmp2_next;
	c1->link.prev = tmp2_prev;

	tmp1_prev->next = &c2->link;
	tmp1_next->prev = &c2->link;
	tmp2_prev->next = &c1->link;
	tmp2_next->prev = &c1->link;
  }

  arrange(c1->mon);
  focusclient(c1,0);
}

void exchange_client(const Arg *arg) {
  Client *c = selmon->sel;
  if (!c || c->isfloating || c->isfullscreen || c->isfakefullscreen || c->isrealfullscreen)
    return;
  exchange_two_client(c, direction_select(arg));
}


void
resize(Client *c, struct wlr_box geo, int interact)
{	

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	struct wlr_box *bbox;
	struct wlr_box clip;
	
	if (!c->mon)
		return;

	bbox = interact ? &sgeom : &c->mon->w;
	
	client_set_bounds(c, geo.width, geo.height); //去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常
	c->geom = geo;
	applybounds(c, bbox);//去掉这个推荐的窗口大小,因为有时推荐的窗口特别大导致平铺异常


	if(c->isnoborder) {
		c->bw = 0;
	}
		
	/* Update scene-graph, including borders */
	wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border[0], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[1], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[2], c->bw, c->geom.height - 2 * c->bw);
	wlr_scene_rect_set_size(c->border[3], c->bw, c->geom.height - 2 * c->bw);
	wlr_scene_node_set_position(&c->border[1]->node, 0, c->geom.height - c->bw);
	wlr_scene_node_set_position(&c->border[2]->node, 0, c->bw);
	wlr_scene_node_set_position(&c->border[3]->node, c->geom.width - c->bw, c->bw);
	wlr_scene_node_set_position(&c->border[1]->node, 0, c->geom.height - c->bw);
	wlr_scene_node_set_position(&c->border[2]->node, 0, c->bw);
	wlr_scene_node_set_position(&c->border[3]->node, c->geom.width - c->bw, c->bw);

	if(!c->isnoclip){
		c->resize = client_set_size(c, c->geom.width - 2 * c->bw,
				c->geom.height - 2 * c->bw);
		client_get_clip(c, &clip);
		wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
	}else {
	/* this is a no-op if size hasn't changed */
	c->resize = client_set_size(c, c->geom.width - 2 * c->bw,
			c->geom.height - 2 * c->bw);
	}
	setborder_color(c);
}

void //17
run(char *startup_cmd)
{
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the startup command */
	autostartexec();
	if (startup_cmd) {
		int piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}
	printstatus();

	/* At this point the outputs are initialized, choose initial selmon based on
	 * cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "left_ptr");

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wl_display_run(dpy);
}

void
setcursor(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a enter
	 * event, which will result in the client requesting set the cursor surface */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client){
		wlr_cursor_set_surface(cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}

}

void //0.5
setfloating(Client *c, int floating)
{	

	Client *p = client_get_parent(c);
	c->isfloating = floating;

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	wlr_scene_node_reparent(&c->scene->node, layers[c->isfullscreen ||
			(p && p->isfullscreen) ? LyrFS
			: c->isfloating ? LyrFloat : LyrTile]);

	if (floating == 1) { 
		if(c->istiled) {
			c->geom.height = c->geom.height * 0.8;
			c->geom.width = c->geom.width * 0.8;
		}
		//重新计算居中的坐标
		c->geom = setclient_coordinate_center(c->geom);
		applyrulesgeom(c);
		resize(c,c->geom,0);
		c->istiled = 0; 
	} else {
		c->istiled = 1; 
		c->is_scratchpad_show = 0;
		c->is_in_scratchpad = 0;
	}

	arrange(c->mon);
	printstatus();
}

void
setfakefullscreen(Client *c, int fakefullscreen)
{
	c->isfakefullscreen = fakefullscreen;
	struct wlr_box  fakefullscreen_box;
	if (!c || !c->mon || !client_surface(c)->mapped)
		return;
	// c->bw = fullscreen ? 0 : borderpx;
	// client_set_fullscreen(c, fullscreen);
	// wlr_scene_node_reparent(&c->scene->node, layers[fullscreen
	// 		? LyrFS : c->isfloating ? LyrFloat : LyrTile]);

	if (fakefullscreen) {
		c->prev = c->geom;
		fakefullscreen_box.x = c->mon->w.x + gappov;
		fakefullscreen_box.y = c->mon->w.y + gappoh;
		fakefullscreen_box.width = c->mon->w.width - 2 * gappov;
		fakefullscreen_box.height = c->mon->w.height - 2 * gappov;
		wlr_scene_node_raise_to_top(&c->scene->node); //将视图提升到顶层
		resize(c, fakefullscreen_box, 0);
		c->isfakefullscreen = 1;
		c->isfloating = 0;
	} else {
		/* restore previous size instead of arrange for floating windows since
		 * client positions are set by the user and cannot be recalculated */
		// resize(c, c->prev, 0);
		c->bw = borderpx;
		c->isfakefullscreen = 0;
		c->isfullscreen = 0;
		c->isrealfullscreen = 0;
		arrange(c->mon);
		client_set_fullscreen(c, false);
	}
}


void
setrealfullscreen(Client *c, int realfullscreen)
{
	c->isrealfullscreen = realfullscreen;

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	if (realfullscreen) {
		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node); //将视图提升到顶层
		resize(c, c->mon->m, 0);
		c->isrealfullscreen = 1;
		c->isfloating = 0;
	} else {
		/* restore previous size instead of arrange for floating windows since
		 * client positions are set by the user and cannot be recalculated */
		// resize(c, c->prev, 0);
		c->bw = borderpx;
		c->isrealfullscreen = 0;
		c->isfullscreen = 0;
		c->isfakefullscreen = 0;
		arrange(c->mon);
		// client_set_fullscreen(c, false);
	}
}

void
setfullscreen(Client *c, int realfullscreen) //用自定义全屏代理自带全屏
{
	c->isrealfullscreen = realfullscreen;

	if (!c || !c->mon || !client_surface(c)->mapped)
		return;

	if (realfullscreen) {
		c->bw = 0;
		wlr_scene_node_raise_to_top(&c->scene->node); //将视图提升到顶层
		resize(c, c->mon->m, 0);
		c->isrealfullscreen = 1;
		c->isfloating = 0;
	} else {
		/* restore previous size instead of arrange for floating windows since
		 * client positions are set by the user and cannot be recalculated */
		// resize(c, c->prev, 0);
		c->bw = borderpx;
		c->isrealfullscreen = 0;
		c->isfullscreen = 0;
		c->isfakefullscreen = 0;
		arrange(c->mon);
	}
}


void
setgaps(int oh, int ov, int ih, int iv)
{
	selmon->gappoh = MAX(oh, 0);
	selmon->gappov = MAX(ov, 0);
	selmon->gappih = MAX(ih, 0);
	selmon->gappiv = MAX(iv, 0);
	arrange(selmon);
}

void //17
setlayout(const Arg *arg)
{
	if (!selmon)
		return;
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt ^= 1;
	if (arg && arg->v) {
		selmon->lt[selmon->sellt] = (Layout *)arg->v;
		selmon->pertag->sellts[selmon->pertag->curtag] = selmon->sellt;
		selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = selmon->lt[selmon->sellt];
	}
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, LENGTH(selmon->ltsymbol));
	arrange(selmon);
	printstatus();
}


void
switch_layout(const Arg *arg)
{
	if (!selmon)
		return;
	
	selmon->sellt ^= 1;
	selmon->pertag->sellts[selmon->pertag->curtag] = selmon->sellt;

	/* TODO change layout symbol? */
	arrange(selmon);
	printstatus();
}

/* arg > 1.0 will set mfact absolutely */
/* arg > 1.0 will set mfact absolutely */
void //17
setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = f;
	arrange(selmon);
}

void //0.5
setmon(Client *c, Monitor *m, uint32_t newtags)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;
	c->mon = m;
	c->prev = c->geom;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0);
		c->tags = newtags ? newtags : m->tagset[m->seltags]; /* assign tags of target monitor */
		setfullscreen(c, c->isfullscreen); /* This will call arrange(c->mon) */
		setfloating(c, c->isfloating);
	}
	focusclient(focustop(selmon), 1);
}

void //17
setpsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void //17
setsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

//获取tags中最坐标的tag的tagmask
unsigned int
get_tags_first_tag(unsigned int tags){
	unsigned int i,target,tag;
	tag = 0;
	for(i=0;!(tag & 1);i++){
		tag = tags >> i;
	}
	target = 1 << (i-1);	
	return target;
}

void show_hide_client(Client *c) {
	unsigned int target = get_tags_first_tag(c->oldtags); 
	tag_client(&(Arg){.ui = target},c);
	// c->tags = c->oldtags;
	c->isminied = 0;
	wlr_foreign_toplevel_handle_v1_set_minimized(c->foreign_toplevel,false);
	focusclient(c,1);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,true);	
}

void
handle_foreign_activate_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_activate_request);
	unsigned int target;
	if(c && !c->isminied && c == selmon->sel) {
		set_minized(c);
		return;
	}

	if(c->isminied) {
		c->is_in_scratchpad = 0;
		c->is_scratchpad_show = 0;
		setborder_color(c);
		show_hide_client(c);
		return;
	}

	target = get_tags_first_tag(c->tags); 
	view(&(Arg){.ui = target});
	focusclient(c,1);
	wlr_foreign_toplevel_handle_v1_set_activated(c->foreign_toplevel,true);

}

void
handle_foreign_fullscreen_request(
		struct wl_listener *listener, void *data) {
	return;
}

void
handle_foreign_close_request(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_close_request);
	if(c) {
		c->iskilling = 1;
		client_send_close(c);
	}
}

void
handle_foreign_destroy(struct wl_listener *listener, void *data) {
	Client *c = wl_container_of(listener, c, foreign_destroy);
	if(c){
		wl_list_remove(&c->foreign_activate_request.link);
		wl_list_remove(&c->foreign_fullscreen_request.link);
		wl_list_remove(&c->foreign_close_request.link);
		wl_list_remove(&c->foreign_destroy.link);
	}
}

void
setup(void)
{
	int i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();
	pointer_manager = wlr_relative_pointer_manager_v1_create(dpy);
	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. The NULL argument here optionally allows you
	 * to pass in a custom renderer if wlr_renderer doesn't meet your needs. The
	 * backend uses the renderer, for example, to fall back to software cursors
	 * if the backend does not support hardware cursors (some older GPUs
	 * don't). */
	if (!(backend = wlr_backend_autocreate(dpy, &session)))
		die("couldn't create backend");

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Create a renderer with the default implementation */
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't create renderer");

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to integrate it
	 * with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_dmabuf_texture_formats(drw)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(scene,
				wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
	}

	/* Create a default allocator */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_data_control_manager_v1_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);
	wlr_subcompositor_create(dpy);

	/* Initializes the interface used to implement urgency hints */
	activation = wlr_xdg_activation_v1_create(dpy);
	LISTEN_STATIC(&activation->events.request_activate, urgent);

	gamma_control_mgr = wlr_gamma_control_manager_v1_create(dpy);
	LISTEN_STATIC(&gamma_control_mgr->events.set_gamma, setgamma);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create();
	LISTEN_STATIC(&output_layout->events.change, updatemons);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&mons);
	LISTEN_STATIC(&backend->events.new_output, createmon);

	/* Set up our client lists and the xdg-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&clients);
	wl_list_init(&fstack);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	LISTEN_STATIC(&idle_inhibit_mgr->events.new_inhibitor, createidleinhibitor);

	layer_shell = wlr_layer_shell_v1_create(dpy, 3);
	LISTEN_STATIC(&layer_shell->events.new_surface, createlayersurface);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	LISTEN_STATIC(&xdg_shell->events.new_surface, createnotify);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &lock_listener);
	LISTEN_STATIC(&session_lock_mgr->events.destroy, destroysessionmgr);
	locked_bg = wlr_scene_rect_create(layers[LyrBlock], sgeom.width, sgeom.height,
			(float [4]){0.1, 0.1, 0.1, 1.0});
	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
			wlr_server_decoration_manager_create(dpy),
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	LISTEN_STATIC(&xdg_decoration_mgr->events.new_toplevel_decoration, createdecoration);
	pointer_constraints = wlr_pointer_constraints_v1_create(dpy);
	LISTEN_STATIC(&pointer_constraints->events.new_constraint, createpointerconstraint);

	relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(dpy);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). Scaled cursors will be loaded with each output. */
	// cursor_mgr = wlr_xcursor_manager_create(cursor_theme, 24);
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	setenv("XCURSOR_SIZE", "24", 1);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in my
	 * input handling blog post:
	 *
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	LISTEN_STATIC(&cursor->events.motion, motionrelative);
	LISTEN_STATIC(&cursor->events.motion_absolute, motionabsolute);
	LISTEN_STATIC(&cursor->events.button, buttonpress);
	LISTEN_STATIC(&cursor->events.axis, axisnotify);
	LISTEN_STATIC(&cursor->events.frame, cursorframe);

	//这两句代码会造成obs窗口里的鼠标光标消失,不知道注释有什么影响
	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	LISTEN_STATIC(&cursor_shape_mgr->events.request_set_shape, setcursorshape);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	wl_list_init(&keyboards);
	LISTEN_STATIC(&backend->events.new_input, inputdevice);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	LISTEN_STATIC(&virtual_keyboard_mgr->events.new_virtual_keyboard, virtualkeyboard);
	virtual_pointer_mgr = wlr_virtual_pointer_manager_v1_create(dpy);
	LISTEN_STATIC(&virtual_pointer_mgr->events.new_virtual_pointer, virtualpointer);

	seat = wlr_seat_create(dpy, "seat0");
	LISTEN_STATIC(&seat->events.request_set_cursor, setcursor);
	LISTEN_STATIC(&seat->events.request_set_selection, setsel);
	LISTEN_STATIC(&seat->events.request_set_primary_selection, setpsel);
	LISTEN_STATIC(&seat->events.request_start_drag, requeststartdrag);
	LISTEN_STATIC(&seat->events.start_drag, startdrag);

	output_mgr = wlr_output_manager_v1_create(dpy);
	LISTEN_STATIC(&output_mgr->events.apply, outputmgrapply);
	LISTEN_STATIC(&output_mgr->events.test, outputmgrtest);

	wlr_scene_set_presentation(scene, wlr_presentation_create(dpy, backend));
#ifdef IM
	/* create text_input-, and input_method-protocol relevant globals */
	input_method_manager = wlr_input_method_manager_v2_create(dpy);
	text_input_manager = wlr_text_input_manager_v3_create(dpy);

	input_relay = calloc(1, sizeof(*input_relay));
	dwl_input_method_relay_init(input_relay);
#endif
    wl_global_create(dpy, &zdwl_ipc_manager_v2_interface, 1, NULL, dwl_ipc_manager_bind);

	//创建顶层管理句柄
	foreign_toplevel_manager =wlr_foreign_toplevel_manager_v1_create(dpy);
	struct wlr_xdg_foreign_registry *foreign_registry = wlr_xdg_foreign_registry_create(dpy);
	wlr_xdg_foreign_v1_create(dpy, foreign_registry);
	wlr_xdg_foreign_v2_create(dpy, foreign_registry);
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	xwayland = wlr_xwayland_create(dpy, compositor, 1);
	if (xwayland) {
		LISTEN_STATIC(&xwayland->events.ready, xwaylandready);
		LISTEN_STATIC(&xwayland->events.new_surface, createnotifyx11);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr, "failed to setup XWayland X server, continuing without it\n");
	}
#endif
}

void
sigchld(int unused)
{
	siginfo_t in;
	/* We should be able to remove this function in favor of a simple
	 *     struct sigaction sa = {.sa_handler = SIG_IGN};
	 *     sigaction(SIGCHLD, &sa, NULL);
	 * but the Xwayland implementation in wlroots currently prevents us from
	 * setting our own disposition for SIGCHLD.
	 */
	/* WNOWAIT leaves the child in a waitable state, in case this is the
	 * XWayland process
	 */
	while (!waitid(P_ALL, 0, &in, WEXITED|WNOHANG|WNOWAIT) && in.si_pid
#ifdef XWAYLAND
			&& (!xwayland || in.si_pid != xwayland->server->pid)
#endif
			) {
		pid_t *p, *lim;
		waitpid(in.si_pid, NULL, 0);
		if (in.si_pid == child_pid)
			child_pid = -1;
		if (!(p = autostart_pids))
			continue;
		lim = &p[autostart_len];

		for (; p < lim; p++) {
			if (*p == in.si_pid) {
				*p = -1;
				break;
			}
		}
	}
}

void //17
spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwl: execvp %s failed:", ((char **)arg->v)[0]);
	}
}


void
startdrag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}


void tag_client(const Arg *arg, Client *target_client) {
	Client *fc;
  	if (target_client  && arg->ui & TAGMASK) {
		target_client->tags = arg->ui & TAGMASK;
  		wl_list_for_each(fc, &clients, link){ 
  			if (fc && fc != target_client && target_client->tags & fc->tags && ISFULLSCREEN(fc) && !target_client->isfloating ) {
  				clear_fullscreen_flag(fc);
  			}
  		}
		view(&(Arg){.ui = arg->ui});

  	} else{
  	  	view(arg);
	}

    focusclient(target_client,1);
	printstatus();	
}

void
tag(const Arg *arg) {
	Client *target_client = selmon->sel;
	tag_client(arg,target_client);
}


void
tagmon(const Arg *arg)
{
	Client *c = focustop(selmon);
	if (c) {
		setmon(c, dirtomon(arg->i), 0);
		reset_foreign_tolevel(c);
		selmon = c->mon;
		c->geom.width = (int)(c->geom.width * selmon->m.width/c->mon->m.width);
		c->geom.height = (int)(c->geom.height * selmon->m.height/c->mon->m.height);
		//重新计算居中的坐标
		if(c->isfloating) {
			c->geom = setclient_coordinate_center(c->geom);
			resize(c,c->geom,0);
		}
		warp_cursor_to_selmon(c->mon);
		focusclient(c,1);
	}

}

void 
overview(Monitor *m, unsigned int gappo, unsigned int gappi) {
	grid(m, overviewgappo, overviewgappi); 
}

// 网格布局窗口大小和位置计算
void grid(Monitor *m, unsigned int gappo, unsigned int gappi) {
  unsigned int i, n;
  unsigned int cx, cy, cw, ch;
  unsigned int dx;
  unsigned int cols, rows, overcols;
  Client *c;
  Client *tempClients[100];
  n = 0;
  wl_list_for_each(c, &clients, link)
  	if (VISIBLEON(c, c->mon) && c->mon == selmon){
			tempClients[n] = c;
    		n++;
  	}
  tempClients[n] = NULL;
  if (n == 0)
    return;
  if (n == 1) {
    c = tempClients[0];
    cw = (m->w.width - 2 * gappo) * 0.7;
    ch = (m->w.height- 2 * gappo) * 0.8;
    resizeclient(c, m->w.x + (m->m.width - cw) / 2, m->w.y + (m->w.height- ch) / 2,
           cw - 2 * c->bw, ch - 2 * c->bw, 0);
    return;
  }
  if (n == 2) {
    c = tempClients[0];
    cw = (m->w.width - 2 * gappo - gappi) / 2;
    ch = (m->w.height- 2 * gappo) * 0.65;
    resizeclient(c, m->m.x + cw + gappo + gappi, m->m.y + (m->m.height - ch) / 2 + gappo,
           cw - 2 * c->bw, ch - 2 * c->bw, 0);
    resizeclient(tempClients[1], m->m.x + gappo, m->m.y + (m->m.height - ch) / 2 + gappo,
           cw - 2 * c->bw, ch - 2 * c->bw, 0);

    return;
  }

  for (cols = 0; cols <= n / 2; cols++)
    if (cols * cols >= n)
      break;
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;
  ch = (m->w.height- 2 * gappo - (rows - 1) * gappi) / rows;
  cw = (m->w.width - 2 * gappo - (cols - 1) * gappi) / cols;

  overcols = n % cols;
  if (overcols)
    dx = (m->w.width - overcols * cw - (overcols - 1) * gappi) / 2 - gappo;
  for (i = 0, c = tempClients[0]; c; c = tempClients[i+1], i++) {
    cx = m->w.x + (i % cols) * (cw + gappi);
    cy = m->w.y + (i / cols) * (ch + gappi);
    if (overcols && i >= n - overcols) {
      cx += dx;
    }
    resizeclient(c, cx + gappo, cy + gappo, cw - 2 * c->bw, ch - 2 * c->bw, 0);
  }
}

// 目标窗口有其他窗口和它同个tag就返回0
unsigned int want_restore_fullscreen(Client *target_client) {
  Client *c = NULL;
  wl_list_for_each(c, &clients, link){ 
    if (c && c != target_client && c->tags == target_client->tags && c == selmon->sel) {
      return 0;
    }
  }

  return 1;
}

// 普通视图切换到overview时保存窗口的旧状态
void overview_backup(Client *c) {
  c->overview_isfloatingbak = c->isfloating;
  c->overview_isfullscreenbak = c->isfullscreen;
  c->overview_isfakefullscreenbak = c->isfakefullscreen;
  c->overview_isrealfullscreenbak = c->isrealfullscreen;
  c->overview_backup_x = c->geom.x;
  c->overview_backup_y = c->geom.y;
  c->overview_backup_w = c->geom.width;
  c->overview_backup_h = c->geom.height;
  c->overview_backup_bw = c->bw;
  if (c->isfloating) {
    c->isfloating = 0;
  }
  if (c->isfullscreen || c->isfakefullscreen ||c->isrealfullscreen ) {
    // if (c->bw == 0) { // 真全屏窗口清除x11全屏属性
    //   XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
    //                   PropModeReplace, (unsigned char *)0, 0);
    // }
    c->isfullscreen = 0; // 清除窗口全屏标志
	c->isfakefullscreen = 0;
	c->isrealfullscreen = 0;
  }
  c->bw = borderpx; // 恢复非全屏的border
}

// overview切回到普通视图还原窗口的状态
void overview_restore(Client *c, const Arg *arg) {
  c->isfloating = c->overview_isfloatingbak;
  c->isfullscreen = c->overview_isfullscreenbak;
  c->isfakefullscreen = c->overview_isfakefullscreenbak;
  c->isrealfullscreen = c->overview_isrealfullscreenbak;
  c->overview_isfloatingbak = 0;
  c->overview_isfullscreenbak = 0;
  c->overview_isfakefullscreenbak = 0;
  c->overview_isrealfullscreenbak = 0;
  c->bw = c->overview_backup_bw;
  if (c->isfloating) {
    // XRaiseWindow(dpy, c->win); // 提升悬浮窗口到顶层
    resizeclient(c, c->overview_backup_x, c->overview_backup_y, c->overview_backup_w,
           c->overview_backup_h, 1);
  } else if (c->isfullscreen ||c->isfakefullscreen ||c->isrealfullscreen) {
	if (want_restore_fullscreen(c)) { //如果同tag有其他窗口,且其他窗口是将要聚焦的,那么不恢复该窗口的全屏状态
    	resizeclient(c, c->overview_backup_x, c->overview_backup_y,
    	             c->overview_backup_w, c->overview_backup_h,1);
	} else {
		c->isfullscreen = 0;
		c->isfakefullscreen = 0;
		c->isrealfullscreen = 0;
	}
  } else {
    resizeclient(c, c->overview_backup_x, c->overview_backup_y, c->overview_backup_w,
           c->overview_backup_h, 0);	
  }

  if(c->bw == 0 && !c->isnoborder && !c->isrealfullscreen) { //如果是在ov模式中创建的窗口,没有bw记录
	c->bw = borderpx;
  }

}


// 显示所有tag 或 跳转到聚焦窗口的tag
void toggleoverview(const Arg *arg) {

	Client *c;
	selmon->isoverview ^= 1;
	unsigned int target;
	unsigned int visible_client_number = 0;

	if(selmon->isoverview){
		wl_list_for_each(c, &clients, link)
		if (c && c->mon == selmon && !c->isminied){
			visible_client_number++;
		}
		if(visible_client_number > 0) {
			target = ~0;
		} else {
			selmon->isoverview ^= 1;
			return;
		}
  	}else if(!selmon->isoverview && selmon->sel) {
		target = get_tags_first_tag(selmon->sel->tags);
  	} else if (!selmon->isoverview && !selmon->sel) {
		target = (1 << (selmon->pertag->prevtag-1));
		view(&(Arg){.ui = target});
		return;
  	}

  	// 正常视图到overview,退出所有窗口的浮动和全屏状态参与平铺,
  	// overview到正常视图,还原之前退出的浮动和全屏窗口状态
  	if (selmon->isoverview) {
  	  	wl_list_for_each(c, &clients, link){
	  		if(c)
  	    		overview_backup(c);
		}
  	} else {
		wl_list_for_each(c, &clients, link){
			if(c)
  	    		overview_restore(c, &(Arg){.ui = target});
		}
  	}

  	view(&(Arg){.ui = target});

}

void
tile(Monitor *m,unsigned int gappo, unsigned int uappi)
{
	unsigned int i, n = 0, h, r, oe = enablegaps, ie = enablegaps, mw, my, ty;
	Client *c;

	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, m) && !c->isfloating && !c->isfullscreen && !c->isrealfullscreen && !c->isfakefullscreen)
			n++;
	if (n == 0)
		return;

	if (smartgaps == n) {
		oe = 0; // outer gaps disabled
	}

	if (n > m->nmaster)
		mw = m->nmaster ? (m->w.width + m->gappiv*ie) * m->mfact : 0;
	else
		mw = m->w.width - 2*m->gappov*oe + m->gappiv*ie;
	i = 0;
	my = ty = m->gappoh*oe;
	wl_list_for_each(c, &clients, link) {
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen ||c->isrealfullscreen || c->isfakefullscreen )
			continue;
		if (i < m->nmaster) {
			r = MIN(n, m->nmaster) - i;
			h = (m->w.height - my - m->gappoh*oe - m->gappih*ie * (r - 1)) / r;
			resize(c, (struct wlr_box){.x = m->w.x + m->gappov*oe, .y = m->w.y + my,
				.width = mw - m->gappiv*ie, .height = h}, 0);
			my += c->geom.height + m->gappih*ie;
		} else {
			r = n - i;
			h = (m->w.height - ty - m->gappoh*oe - m->gappih*ie * (r - 1)) / r;
			resize(c, (struct wlr_box){.x = m->w.x + mw + m->gappov*oe, .y = m->w.y + ty,
				.width = m->w.width - mw - 2*m->gappov*oe, .height = h}, 0);
			ty += c->geom.height + m->gappih*ie;
		}
		i++;
	}
}


void
togglefloating(const Arg *arg)
{
	Client *sel = focustop(selmon);

	if(!sel)
		return;

  	if (sel->isfullscreen || sel->isfakefullscreen ||sel->isrealfullscreen ) {
  		sel->isfullscreen = 0; // 清除窗口全屏标志
		sel->isfakefullscreen = 0;
		sel->isrealfullscreen = 0;
		sel->bw = borderpx; // 恢复非全屏的border
  	}
	/* return if fullscreen */
	setfloating(sel, !sel->isfloating);
	setborder_color(sel);
}

void
togglefullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel)
		return;
	if(sel->isfloating)
		setfloating(sel, 0);

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
	setfullscreen(sel, !sel->isfullscreen);
}

void
togglefakefullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if(!sel)
		return;

	if(sel->isfloating)
		setfloating(sel, 0);

	if (sel->isfullscreen || sel->isfakefullscreen || sel->isrealfullscreen)
		setfakefullscreen(sel, 0);
	else
		setfakefullscreen(sel, 1);

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
}

void
togglerealfullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if(!sel)
		return;

	if(sel->isfloating)
		setfloating(sel, 0);

	if (sel->isfullscreen || sel->isfakefullscreen || sel->isrealfullscreen)
		setrealfullscreen(sel, 0);
	else
		setrealfullscreen(sel, 1);

	sel->is_scratchpad_show = 0;
	sel->is_in_scratchpad = 0;
}

void
togglegaps(const Arg *arg)
{
	enablegaps ^= 1;
	arrange(selmon);
}

void
toggletag(const Arg *arg)
{
	unsigned int newtags;
	Client *sel = focustop(selmon);
	if (!sel)
		return;
	newtags = sel->tags ^ (arg->ui & TAGMASK);
	if (newtags) {
		sel->tags = newtags;
		focusclient(focustop(selmon), 1);
		arrange(selmon);
	}
	printstatus();
}

void
toggleview(const Arg *arg)
{
	unsigned int newtagset = selmon ? selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK) : 0;

	if (newtagset) {
		selmon->tagset[selmon->seltags] = newtagset;
		focusclient(focustop(selmon), 1);
		arrange(selmon);
	}
	printstatus();
}

void // 0.5
unlocksession(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void // 0.5
unmaplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;
	wlr_scene_node_set_enabled(&l->scene->node, 0);
	if (l == exclusive_focus)
		exclusive_focus = NULL;
	if (l->layer_surface->output && (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);
	if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void
unmapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is unmapped, and should no longer be shown. */
	Client *c = wl_container_of(listener, c, unmap);

	if (c == selmon->sel)
		selmon->sel = NULL;

	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (client_is_unmanaged(c)) {
		if (c == exclusive_focus)
			exclusive_focus = NULL;
		if (client_surface(c) == seat->keyboard_state.focused_surface)
			focusclient(focustop(selmon), 1);
	} else {
		wl_list_remove(&c->link);
		setmon(c, NULL, 0);
		wl_list_remove(&c->flink);
	}

	wlr_scene_node_destroy(&c->scene->node);
	if(c->foreign_toplevel){
		wlr_foreign_toplevel_handle_v1_destroy(c->foreign_toplevel);
		c->foreign_toplevel = NULL;
	}

	Client *nextfocus = focustop(selmon);

	if(nextfocus) {
		focusclient(nextfocus,0);
	}

	if(!nextfocus && selmon->isoverview){
		Arg arg = {0};
		toggleoverview(&arg);
	}

	printstatus();
	motionnotify(0, NULL, 0, 0, 0, 0);
}

void //0.5
updatemons(struct wl_listener *listener, void *data)
{
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *config
			= wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);
		config_head->state.enabled = 0;
		/* Remove this output from the layout to avoid cursor enter inside it */
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){0};
	}
	/* Insert outputs that need to */
	wl_list_for_each(m, &mons, link) {
		if (m->wlr_output->enabled
				&& !wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout, m->wlr_output);
	}

	/* Now that we update the output layout we can get its box */
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	/* Make sure the clients are hidden when dwl is locked */
	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link) {
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(config, m->wlr_output);

		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		wlr_scene_node_set_position(&m->fullscreen_bg->node, m->m.x, m->m.y);
		wlr_scene_rect_set_size(m->fullscreen_bg, m->m.width, m->m.height);

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree = m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
			wlr_session_lock_surface_v1_configure(m->lock_surface, m->m.width, m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0);

		/* Try to re-set the gamma LUT when updating monitors,
		 * it's only really needed when enabling a disabled output, but meh. */
		m->gamma_lut_changed = 1;

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;
		if (!selmon) {
			selmon = m;
		}
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link) {
			if (!c->mon && client_surface(c)->mapped) {
				setmon(c, selmon, c->tags);
				reset_foreign_tolevel(c);
			}
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
					wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface, 1);
		}
	}

	/* FIXME: figure out why the cursor image is at 0,0 after turning all
	 * the monitors on.
	 * Move the cursor image where it used to be. It does not generate a
	 * wl_pointer.motion event for the clients, it's only the image what it's
	 * at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

void
updatetitle(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_title);
	const char *title;
    title = client_get_title(c) ;
	if(title && c->foreign_toplevel)
		wlr_foreign_toplevel_handle_v1_set_title(c->foreign_toplevel,title);
	if (c == focustop(c->mon))
		printstatus();

}

void //17 fix to 0.5
urgent(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);

	if (!c || !c->foreign_toplevel)
		return;

	if (focus_on_activate && c != selmon->sel) {
		view(&(Arg){.ui = c->tags});
		focusclient(c,1);
	} else if(c != focustop(selmon)) {
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);
		c->isurgent = 1;
		printstatus();
	}
}

void
view(const Arg *arg)
{
	size_t i, tmptag;

	if(!selmon || (arg->ui != ~0 && selmon->isoverview)){
		return;
	}

	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++) ;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}


void
viewtoleft(const Arg *arg)
{
	size_t tmptag;
	unsigned int target = selmon->tagset[selmon->seltags];

	if(selmon->isoverview || selmon->pertag->curtag == 0){
		return;
	}

	target >>= 1;

	if(target == 0){
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag - 1;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
viewtoright_have_client(const Arg *arg)
{
	size_t tmptag;
	Client *c;
	unsigned int found = 0;
	unsigned int n = 1;
	unsigned int target = selmon->tagset[selmon->seltags];

	if(selmon->isoverview || selmon->pertag->curtag == 0){
		return;
	}

	for(target <<= 1;target & TAGMASK;target <<= 1,n++){
		wl_list_for_each(c, &clients, link){
			if(target & c->tags){
				found = 1;
				break;
			}
		}
		if(found){
			break;
		}
	}

	if(!(target & TAGMASK)){
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag + n;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
viewtoright(const Arg *arg)
{
	if(selmon->isoverview || selmon->pertag->curtag == 0){
		return;
	}
	size_t tmptag;
	unsigned int target = selmon->tagset[selmon->seltags];
	target <<= 1;

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	if (!(target & TAGMASK)){
		return;
	}
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag + 1;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void
viewtoleft_have_client(const Arg *arg)
{
	size_t tmptag;
	Client *c;
	unsigned int found = 0;
	unsigned int n = 1;
	unsigned int target = selmon->tagset[selmon->seltags];

	if(selmon->isoverview || selmon->pertag->curtag == 0){
		return;
	}

	for(target >>= 1;target>0;target >>= 1,n++){
		wl_list_for_each(c, &clients, link){
			if(target & c->tags){
				found = 1;
				break;
			}
		}
		if(found){
			break;
		}
	}

	if(target == 0){
		return;
	}

	if (!selmon || (target) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (target) {
		selmon->tagset[selmon->seltags] = target;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = selmon->pertag->curtag - n;
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}


void tagtoleft(const Arg *arg) {
  if (selmon->sel != NULL &&
      __builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
      selmon->tagset[selmon->seltags] > 1) {
    tag(&(Arg){.ui = selmon->tagset[selmon->seltags] >> 1});
  }
}

void tagtoright(const Arg *arg) {
  if (selmon->sel != NULL &&
      __builtin_popcount(selmon->tagset[selmon->seltags] & TAGMASK) == 1 &&
      selmon->tagset[selmon->seltags] & (TAGMASK >> 1)) {
    tag(&(Arg){.ui = selmon->tagset[selmon->seltags] << 1});
  }
}


void
virtualkeyboard(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	createkeyboard(&keyboard->keyboard);
}

void
warp_cursor(const Client *c) {
	if (cursor->x < c->geom.x ||
		cursor->x > c->geom.x + c->geom.width ||
		cursor->y < c->geom.y ||
		cursor->y > c->geom.y + c->geom.height)
		wlr_cursor_warp_closest(cursor,
			  NULL,
			  c->geom.x + c->geom.width / 2.0,
			  c->geom.y + c->geom.height / 2.0);
}

void
warp_cursor_to_selmon(const Monitor *m) {

	wlr_cursor_warp_closest(cursor,
			  NULL,
			  m->w.x + m->w.width / 2.0,
			  m->w.y + m->w.height / 2.0);
}

void
virtualpointer(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_pointer_v1_new_pointer_event *event = data;
	struct wlr_input_device *device = &event->new_pointer->pointer.base;

	wlr_cursor_attach_input_device(cursor, device);
	if (event->suggested_output)
		wlr_cursor_map_input_to_output(cursor, device, event->suggested_output);
}

Monitor *
xytomon(double x, double y)
{
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

void
xytonode(double x, double y, struct wlr_surface **psurface,
		Client **pc, LayerSurface **pl, double *nx, double *ny)
{
	struct wlr_scene_node *node, *pnode;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int layer;

	for (layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {
	#ifdef IM
		        if (layer == LyrIMPopup) continue;
	#endif
		if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx, ny)))
			continue;

		if (node->type == WLR_SCENE_NODE_BUFFER)
			surface = wlr_scene_surface_try_from_buffer(
					wlr_scene_buffer_from_node(node))->surface;
		/* Walk the tree to find a node that knows the client */
		for (pnode = node; pnode && !c; pnode = &pnode->parent->node)
			c = pnode->data;
		if (c && c->type == LayerShell) {
			c = NULL;
			l = pnode->data;
		}
	}

	if (psurface) *psurface = surface;
	if (pc) *pc = c;
	if (pl) *pl = l;
}

void toggleglobal(const Arg *arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->is_in_scratchpad) {
    selmon->sel->is_in_scratchpad = 0;
    selmon->sel->is_scratchpad_show = 0;
  } 
  selmon->sel->isglobal ^= 1;
//   selmon->sel->tags =
//       selmon->sel->isglobal ? TAGMASK : selmon->tagset[selmon->seltags];
//   focustop(selmon);
  setborder_color(selmon->sel);
}

void
zoom(const Arg *arg)
{
	Client *c, *sel = focustop(selmon);

	if (!sel || !selmon || !selmon->lt[selmon->sellt]->arrange || sel->isfloating)
		return;

	/* Search for the first tiled window that is not sel, marking sel as
	 * NULL if we pass it along the way */
	wl_list_for_each(c, &clients, link)
		if (VISIBLEON(c, selmon) && !c->isfloating) {
			if (c != sel)
				break;
			sel = NULL;
		}

	/* Return if no other tiled window was found */
	if (&c->link == &clients)
		return;

	/* If we passed sel, move c to the front; otherwise, move sel to the
	 * front */
	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon);
}

#ifdef XWAYLAND
void
activatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, activate);

	/* Only "managed" windows can be activated */
	if (!client_is_unmanaged(c))
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);

	if (!c || !c->foreign_toplevel)
		return;

	if (focus_on_activate && c != selmon->sel) {
		view(&(Arg){.ui = c->tags});
		focusclient(c,1);
	} else if(c != focustop(selmon)) {
		if (client_surface(c)->mapped)
			client_set_border_color(c, urgentcolor);   //在使用窗口剪切补丁后,这里启动gdm-settings的字体更改那里点击就会崩溃,增加过滤条件为是toplevel窗口后似乎已经解决
		c->isurgent = 1;
		printstatus();
	}
}

void //0.7
configurex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!client_surface(c) || !client_surface(c)->mapped) {
		wlr_xwayland_surface_configure(c->surface.xwayland,
				event->x, event->y, event->width, event->height);
		return;
	}
	if (client_is_unmanaged(c)) {
		wlr_scene_node_set_position(&c->scene->node, event->x, event->y);
		wlr_xwayland_surface_configure(c->surface.xwayland,
				event->x, event->y, event->width, event->height);
		return;
	}
	if ((c->isfloating && c != grabc) || !c->mon->lt[c->mon->sellt]->arrange)
		resize(c, (struct wlr_box){.x = event->x - c->bw, .y = event->y - c->bw,
				.width = event->width + c->bw * 2, .height = event->height + c->bw * 2}, 0);
	else
		arrange(c->mon);
}

/*创建窗口监测函数*/
void
createnotifyx11(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	c->bw = borderpx;

	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure, configurex11);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen, fullscreennotify);
	LISTEN(&xsurface->events.request_maximize, &c->maximize, maximizenotify);
	LISTEN(&xsurface->events.request_minimize, &c->minimize, minimizenotify);
}

Atom
getatom(xcb_connection_t *xc, const char *name)
{
	Atom atom = 0;
	xcb_intern_atom_reply_t *reply;
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(xc, 0, strlen(name), name);
	if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
		atom = reply->atom;
	free(reply);

	return atom;
}

void
sethints(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_hints);
	if (c != focustop(selmon)) {
		c->isurgent = xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
		printstatus();
	}
}

void
xwaylandready(struct wl_listener *listener, void *data)
{
	struct wlr_xcursor *xcursor;
	xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
	int err = xcb_connection_has_error(xc);
	if (err) {
		fprintf(stderr, "xcb_connect to X server failed with code %d\n. Continuing with degraded functionality.\n", err);
		return;
	}

	/* Collect atoms we are interested in.  If getatom returns 0, we will
	 * not detect that window type. */
	netatom[NetWMWindowTypeDialog] = getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
	netatom[NetWMWindowTypeSplash] = getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
	netatom[NetWMWindowTypeToolbar] = getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
	netatom[NetWMWindowTypeUtility] = getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* Set the default XWayland cursor to match the rest of dwl. */
	if ((xcursor = wlr_xcursor_manager_get_xcursor(cursor_mgr, "left_ptr", 1)))
		wlr_xwayland_set_cursor(xwayland,
				xcursor->images[0]->buffer, xcursor->images[0]->width * 4,
				xcursor->images[0]->width, xcursor->images[0]->height,
				xcursor->images[0]->hotspot_x, xcursor->images[0]->hotspot_y);

	xcb_disconnect(xc);
}
#endif

int
main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:hdv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'd')
			log_level = WLR_DEBUG;
		else if (c == 'v')
			die("dwl " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications socket */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-d] [-s startup command]", argv[0]);
}