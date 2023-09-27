/* speedie's dwl config */

/* appearance */
static const unsigned int hotarea_size              = 10;
static const unsigned int enable_hotarea            = 1;
static int smartgaps                 = 0;  /* 1 means no outer gap when there is only one window */
static int sloppyfocus               = 1;  /* focus follows mouse */
static unsigned int gappih           = 5; /* horiz inner gap between windows */
static unsigned int gappiv           = 5; /* vert inner gap between windows */
static unsigned int gappoh           = 10; /* horiz outer gap between windows and screen edge */
static unsigned int gappov           = 10; /* vert outer gap between windows and screen edge */
static int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static unsigned int borderpx         = 5;  /* border pixel of windows */
static const float rootcolor[]             = { 0.3, 0.3, 0.3, 1.0 };
static const float bordercolor[]           = { 0, 0, 0, 0 };
static const float focuscolor[]            = { 0.6, 0.4, 0.1, 1 };
static const float fakefullscreencolor[]   = { 0.1, 0.5, 0.2, 1 };

static const int overviewgappi = 24; /* overview时 窗口与边缘 缝隙大小 */
static const int overviewgappo = 60; /* overview时 窗口与窗口 缝隙大小 */

/* To conform the xdg-protocol, set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.1, 0.1, 0.1, 1.0};

static int warpcursor = 0; /* Warp cursor to focused client */

/* Autostart */
static const char *const autostart[] = {
    "/bin/sh", "-c", "$HOME/.config/hypr/autostart.sh", NULL,
    NULL,
};

/* tagging
 * expand the array to add more tags
 */
static const char *tags[] = {
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
};

static const Rule rules[] = {
	/* app_id     title       tags mask     isfloating   monitor */
	/* examples:
	{ "Gimp",     NULL,       0,            1,           -1 },
	*/
	{ "Google-chrome",  NULL,       1 << 3,       0,           -1 },
	{ "Clash for Windows",  NULL,       0,       1,           -1 },
	{ "electron-netease-cloud-music",  NULL,       0,       1,           -1 },
	{ NULL,  "图片查看器",       0,       1,           -1 },
	{ NULL,  "图片查看",       0,       1,           -1 },

};

/* layout(s) */
static const Layout overviewlayout = { "󰃇",  overview };

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* monitors */
static const MonitorRule monrules[] = {
	/* name       mfact nmaster scale layout       rotate/reflect */
	/* example of a HiDPI laptop monitor:
	{ "eDP-1",    0.5,  1,      2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL },
	*/
	/* defaults */
	{ NULL,       0.55, 1,      1,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
};

static int repeat_rate = 25;
static int repeat_delay = 600;

/* Trackpad */
static int tap_to_click = 1;
static int tap_and_drag = 1;
static int drag_lock = 1;
static int natural_scrolling = 0;
static int disable_while_typing = 1;
static int left_handed = 0;
static int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;
/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_ALT

#define TAGKEYS(KEY,SKEY,TAG) \
	{ WLR_MODIFIER_CTRL,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,            toggleview,      {.ui = 1 << TAG} }, \
	{ WLR_MODIFIER_ALT, KEY,           tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT,SKEY,toggletag, {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[] = { "konsole", NULL };
static const char *menucmd[] = { "wofi", NULL };
static const char *webcmd[] = { "google-chrome", NULL };

static const Key keys[] = {
	/* Note that Shift changes certain key codes: c -> C, 2 -> at, etc. */
	/* modifier                  key                 function        argument */
	{ MODKEY,					 XKB_KEY_space,      spawn,          {.v = menucmd } },
	{ MODKEY, 					 XKB_KEY_Return,     spawn,          {.v = termcmd } },
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_W,          spawn,          {.v = webcmd } },
    { WLR_MODIFIER_LOGO,         XKB_KEY_Return, 		 spawn, SHCMD("google-chrome") },
    { WLR_MODIFIER_CTRL,         XKB_KEY_Return, 		 spawn, SHCMD("bash ~/tool/clash.sh") },  
    { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,         XKB_KEY_a, 		 spawn, SHCMD("grim -g \"$(slurp)\" - | swappy -f -") }, 

	{ WLR_MODIFIER_LOGO,                    	XKB_KEY_Tab,          focusstack,     {.i = +1} },
	{ WLR_MODIFIER_LOGO|WLR_MODIFIER_SHIFT,   	XKB_KEY_Tab,          focusstack,     {.i = -1} },
  	{ WLR_MODIFIER_ALT,                  XKB_KEY_k,            focusdir,         {.i = UP } },              /* alt k              | 二维聚焦窗口 */
  	{ WLR_MODIFIER_ALT,                  XKB_KEY_j,            focusdir,         {.i = DOWN } },            /* alt j              | 二维聚焦窗口 */
  	{ WLR_MODIFIER_ALT,                  XKB_KEY_h,            focusdir,         {.i = LEFT } },            /* alt h              | 二维聚焦窗口 */
  	{ WLR_MODIFIER_ALT,                  XKB_KEY_l,            focusdir,         {.i = RIGHT } },           /* alt l              | 二维聚焦窗口 */
    { WLR_MODIFIER_ALT,                  XKB_KEY_Left,         focusdir,         {.i = LEFT } },            /* alt left           |  本tag内切换聚焦窗口 */
    { WLR_MODIFIER_ALT,                  XKB_KEY_Right,        focusdir,         {.i = RIGHT } },           /* alt right          |  本tag内切换聚焦窗口 */
    { WLR_MODIFIER_ALT,                  XKB_KEY_Up,           focusdir,         {.i = UP } },              /* alt up             |  本tag内切换聚焦窗口 */
    { WLR_MODIFIER_ALT,                  XKB_KEY_Down,         focusdir,         {.i = DOWN } },    

	{ MODKEY,                    XKB_KEY_i,          incnmaster,     {.i = +1} },
	{ MODKEY,                    XKB_KEY_n,          incnmaster,     {.i = -1} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_H,          setmfact,       {.f = -0.05} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_L,          setmfact,       {.f = +0.05} },
	{ MODKEY,                    XKB_KEY_s,          zoom,           {0} },
	{ MODKEY,                    XKB_KEY_Tab,        view,           {0} },
	{ WLR_MODIFIER_LOGO,                    XKB_KEY_a,          toggleoverview,           {0} },
	{ WLR_MODIFIER_CTRL,                    XKB_KEY_Left,        viewtoleft,           {0} },
	{ WLR_MODIFIER_CTRL,                    XKB_KEY_Right,        viewtoright,           {0} },
    { WLR_MODIFIER_CTRL|WLR_MODIFIER_LOGO,    XKB_KEY_Left,         tagtoleft,        {0} },                     /* ctrl alt left      |  将本窗口移动到左边tag */
    { WLR_MODIFIER_CTRL|WLR_MODIFIER_LOGO,    XKB_KEY_Right,        tagtoright,       {0} }, 
	{ MODKEY,					 XKB_KEY_q,          killclient,     {0} },
	{ MODKEY,                    XKB_KEY_t,          setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                    XKB_KEY_e,          setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                    XKB_KEY_m,          setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                    XKB_KEY_space,      setlayout,      {0} },
	{ WLR_MODIFIER_ALT, XKB_KEY_backslash,      togglefloating, {0} },
	{ MODKEY,                    XKB_KEY_a,          togglefakefullscreen, {0} },
	{ MODKEY,                    XKB_KEY_f,          togglerealfullscreen, {0} },
	{ WLR_MODIFIER_CTRL,                    XKB_KEY_KP_0,          view,           {.ui = ~0} },
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_LOGO, XKB_KEY_KP_0, tag,            {.ui = ~0} },
	{ MODKEY,                    XKB_KEY_comma,      focusmon,       {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_period,     focusmon,       {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_less,       tagmon,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_greater,    tagmon,         {.i = WLR_DIRECTION_RIGHT} },
    { MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_X,          incgaps,        {.i = +1 } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Z,          incgaps,        {.i = -1 } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_0,          togglegaps,     {0} },
	TAGKEYS(          XKB_KEY_KP_1, XKB_KEY_exclam,                     0),
	TAGKEYS(          XKB_KEY_KP_2, XKB_KEY_at,                         1),
	TAGKEYS(          XKB_KEY_KP_3, XKB_KEY_numbersign,                 2),
	TAGKEYS(          XKB_KEY_KP_4, XKB_KEY_dollar,                     3),
	TAGKEYS(          XKB_KEY_KP_5, XKB_KEY_percent,                    4),
	TAGKEYS(          XKB_KEY_KP_6, XKB_KEY_asciicircum,                5),
	TAGKEYS(          XKB_KEY_KP_7, XKB_KEY_ampersand,                  6),
	TAGKEYS(          XKB_KEY_KP_8, XKB_KEY_asterisk,                   7),
	TAGKEYS(          XKB_KEY_KP_9, XKB_KEY_parenleft,                  8),
	//{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q,          quit,           {0} },

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
	{ WLR_MODIFIER_LOGO,XKB_KEY_m, quit, {0} },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ WLR_MODIFIER_LOGO, BTN_LEFT,   moveresize,     {.ui = CurMove } },
	{ 0, BTN_MIDDLE, togglefakefullscreen, {0} }, //中键触发假全屏
	{ WLR_MODIFIER_LOGO, BTN_RIGHT,  moveresize,     {.ui = CurResize } },
	{ 0, BTN_LEFT,  toggleoverview,     {0} },
	{ 0, BTN_RIGHT,  killclient,     {0} },
};

static const Wheel wheels[] = {
	{ WLR_MODIFIER_LOGO, WheelUp, viewtoleft, {0} }, //中键+super向前切换工作区
	{ WLR_MODIFIER_LOGO, WheelDown, viewtoright, {0} }, //中键+super向后切换工作区
};