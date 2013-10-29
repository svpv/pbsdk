#ifndef INKINTERNAL_H
#define INKINTERNAL_H

#include "inkview.h"

#ifndef EMULATOR
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#else
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#define iverror(x...) fprintf(stderr, x);
//#define IVDBG(x...) fprintf(stderr, x);
#define IVDBG(x...)

#define MAXPATHLENGTH 1024
#define MAXEVENTS 64
#define MAXDICTS 64
#define SLEEPDELAY 1500LL
#define EINKDELAY 1500LL
#define MAXVOL 36

#define SHOWCLOCK_ONTURN 0
#define SHOWCLOCK_NOHIDE 1
#define SHOWCLOCK_ALWAYS 2
#define SHOWCLOCK_OFF 3

#define PWRACT_LAST 0
#define PWRACT_LOCK 1
#define PWRACT_MMNU 2
#define PWRACT_EXIT 3
#define PWRACT_SCRN 4
#define PWRACT_PROF 5
#define PWRACT_STTY 30
#define PWRACT_NONE 31

#define DEEPSLEEPTIME 600000LL
#define BATTERYCHECKTIME 10800000LL

#define SN_ORSIO "92457"

#define C24TO8(c) (((((c) >> 16) & 255) * 77 + (((c) >> 8) & 255) * 151 + ((c) & 255) * 28) >> 8)

/*

 [ 63 62 61 60 | 59 58 57 56 | 55 54 53 52 | 51 50 49 48 | 47 46 45 44 | 43 42 41 40 | 39 38 37 36 | 35 34 33 32 ]
 .             .             .  captouch   .           * .    WiFi module     .  bluetooth module  . touchscreen .
 .             .             . 0=none      .           | .   0=none           . 0=none             . 0=none      .
 .             .             . 1=AD7147/1  .           | .   1=WG7210         . 1=WG7210           . 1=S3C-resist.
 .             .             .             .           | .   2=GM9601         .                    .             .
 .             .             .             .           | .                    .                    .             .
 .             .             .             .           `usb host              .                    .             .
 .             .             .             .             .                    .                    .             .

 [ 31 30 29 28 | 27 26 25 24 | 23 22 21 20 | 19 18 17 16 | 15 14 13 12 | 11 10  9  8 |  7  6  5  4 |  3  2  1  0 ]
 .  USB        .  audio      .  g-sensor   .  *+* .   keyboard         .  controller .  display    .  board type .
 . 0=none      . 0=none      . 0=none      .   |  .  0=6"ntx           . 0=PVI       . 0=6"600x800 . 0=EB100     .
 . 1=ISP1582   . 1=UDA1345   . 1=MMA7455/1 .   |  .  1=5"pb360         . 1=Epson/GP  . 1=5"600x800 . 1=EB600     .
 . 2=RTS5105   . 2=ALC5623   . 2=MMA7455/2 .   |  .  2=cookie          . 2=Epson/M   . 2=8"1024x768. 2=EB500     .
 .             .             .             .   |  .  3=9d7linear       .             . 3=9"1200x825. 3=COOKIE    .
 .             .             .             .   |  .                    .             .             .             .
 .             .             .             .   `display orientation (1=topdown)      .             .             .

*/

#define ISLTRALPHA(c)  (((c) >= 0x41 && (c) <= 0x5a)     || \
			((c) >= 0x61 && (c) <= 0x7a)     || \
			((c) >= 0xc0 && (c) <= 0x2b8)    || \
			((c) >= 0x386 && (c) <= 0x587))

#define ISRTLALPHA(c)  (((c) >= 0x591 && (c) <= 0x6ff)   || \
			((c) >= 0xfb1d && (c) <= 0xfdff))

#define ISRTLPUNCT(c)  (((c) >= 0x20 && (c) <= 0x2f)     || \
			((c) >= 0x3a && (c) <= 0x3f)     || \
			((c) >= 0xa1 && (c) <= 0xbf)     || \
			((c) >= 0x2010 && (c) <= 0x2046))

#define ISRTLDIGIT(c) (((c) >= '0' && (c) <= '9') || ((c) == '%'))

typedef struct iv_glyph_s {

	unsigned short chr;
	char left;
	char top;
	unsigned char width;
	unsigned char height;
	unsigned char data[0];

} iv_glyph;

typedef struct iv_state_s {

	int isopen;
	char *theme;
	char *lang;
	char *kbdlang;
	unsigned char pwract1;
	unsigned char pwract2;
	unsigned char timeformat;
	unsigned char gsense;
	unsigned char showclock;
	unsigned char _reserved_00;
	unsigned char keylock;
	unsigned char demo;
	int antialiasing;

	pid_t pid;
	iv_handler hproc;
	iv_handler mainhproc;

	icanvas *framebuffer;
	icanvas *canvas;
	int orientation;

	iconfig *cfg;
	FT_Library ftlibrary;
	ihash *fonthash;
	ifont *current_font;
	FT_Face current_face;
	int current_color;
	int current_aa;
	unsigned char **cw_cache;
	unsigned short **kerning_cache;
	iv_glyph **glyph_cache;

	char *font;
	char *fontb;
	char *fonti;
	char *fontbi;

	int keylocktm;
	int powerofftm;
	int allowkeyup;
	int currentkey;
	int keyrepeats;
	int initialized;
	int exiting;
	int usbmode;
	char inplayer;
	char inlastopen;
	char in_dialog;
	char in_menu;
	int nohourglass;
	int needupdate;
	int poweroffenable;
	int sleepenable;
	long long waketime;
	long long sleeptime;
	long long measuretime;
	
	itimer *timers;
	int ntimers;

	ievent *events;
	int evtstart;
	int evtend;

	int keytime1;
	int keytime2;
	int msgtop;

	int ptrx;
	int ptry;

	ifont **fontlist;
	int fontcount;

	struct {
		int raw;
		int a;
		int b;
		int c;
		int d;
		int e;
		int f;
		int nx;
		int ny;
	} tsc;

	FT_Face current_zface;

} iv_state;

typedef struct iv_caches_s {

	int size;
	int refs;
	unsigned char **cw_cache;
	unsigned short **kerning_cache;
	iv_glyph **glyph_cache_aa;
	iv_glyph **glyph_cache_noaa;

} iv_caches;

typedef struct iv_fontdata_s {

	int refs;
	FT_Face face;
	FT_Face zface;
	int nglyphs;
	int nsizes;
	iv_caches *caches;

} iv_fontdata;

typedef struct iv_mpctl_s {

	unsigned char pclink;
	unsigned char poweroff;
	unsigned char lockdisplay;
	unsigned char remountfs;
	unsigned char restart;
	unsigned char wakeup;
	unsigned char safe_mode;
	unsigned char inkflags;
	unsigned char gorient;
	unsigned char keylock;
	unsigned char usbmounted;
	unsigned char umoverride;

	int monpid;
	int bspid;
	int apppid;
	int mppid;
	int nagtpid;
	int reserved_06;
	int reserved_07;
	int power;
	int pkey;
	int vid;

	short state;
	short mode;
	short track;
	short reserved_0b;

	int tracksize;
	int position;
	int newposition;

	short volume;
	short equalizer[32];
	short filter_changed;
	short reserved_0c;

	unsigned char btready;
	unsigned char wifiready;
	unsigned char connected;
	unsigned char rfcomm;

	char netname[64];
	char netdevice[16];
	char netsecurity[16];
	char netprefix[16];
	int con_index;
	int con_time;
	int con_speed;
	int reserved_0e;
	unsigned long bytes_in;
	unsigned long bytes_out;
	unsigned long packets_in;
	unsigned long packets_out;

} iv_mpctl;

typedef struct eink_cmd_s {

	int owner;
	int command;
	int nwrite;
	int nread;
	unsigned char data[0];

} eink_cmd;

extern iv_state ivstate;
extern iv_mpctl *ivmpc;

extern ifont *title_font, *window_font, *header_font, *menu_s_font, *menu_n_font;
extern ifont *butt_n_font, *butt_s_font;
extern int window_color, header_color, menu_n_color, menu_s_color, menu_d_color;
extern int butt_n_color, butt_s_color;
extern ibitmap *button_normal, *button_selected;
extern ibitmap *button_back, *button_prev, *button_next;
extern int title_height;

extern ibitmap def_icon_question, def_icon_information, def_icon_warning, def_icon_error;
extern ibitmap def_fileicon, def_folder;
extern ibitmap def_hourglass, def_lock;
extern ibitmap def_button_normal, def_button_selected;
extern ibitmap def_pageentry;
extern ibitmap def_bmk_panel, def_bmk_active, def_bmk_inactive, def_bmk_actnew;
extern ibitmap def_tab_panel, def_tab_active, def_tab_inactive;
extern ibitmap def_leaf_open, def_leaf_closed;
extern ibitmap def_battery_0, def_battery_1, def_battery_2, def_battery_3, def_battery_4;
extern ibitmap def_battery_5, def_battery_c;
extern ibitmap def_button_back, def_button_prev, def_button_next;
extern ibitmap keyboard_arrow, keyboard_bs, tsk_arrow, tsk_bs, tsk_enter, tsk_can, tsk_clr;

extern ibitmap def_p_book, def_p_loading;
extern ibitmap def_playing, def_usbinfo, def_gprsinfo, def_wifiinfo, def_usbex;
extern ibitmap def_dicmenu;

extern unsigned long long hwconfig;
extern char hw_platform[];

#define HWCONFIG_DEFAULT 0x0000000021000000LL
#define HWC_PLATFORM     ((int)(hwconfig & 15))
#define HWC_DISPLAY      ((int)((hwconfig >> 4) & 15))
#define HWC_CONTROLLER   ((int)((hwconfig >> 8) & 15))
#define HWC_KEYBOARD     ((int)((hwconfig >> 12) & 31))
#define HWC_GSENSOR      ((int)((hwconfig >> 20) & 15))
#define HWC_AUDIO        ((int)((hwconfig >> 24) & 15))
#define HWC_USB          ((int)((hwconfig >> 28) & 15))
#define HWC_TOUCHPANEL   ((int)((hwconfig >> 32) & 15))
#define HWC_BLUETOOTH    ((int)((hwconfig >> 36) & 63))
#define HWC_WIFI         ((int)((hwconfig >> 42) & 63))
#define HWC_USBHOST      ((int)((hwconfig >> 48) & 1))
#define HWC_DPOS         ((int)((hwconfig >> 18) & 3))
#define HWC_CAPTOUCH     ((int)((hwconfig >> 52) & 15))

#define HWC_EB100 (HWC_PLATFORM == 0)
#define HWC_EB600 (HWC_PLATFORM == 1)
#define HWC_EB500 (HWC_PLATFORM == 2)
#define HWC_COOKIE (HWC_PLATFORM == 3)

#define HWC_KEYBOARD_NTX600 (HWC_KEYBOARD == 0)
#define HWC_KEYBOARD_POCKET360 (HWC_KEYBOARD == 1)
#define HWC_KEYBOARD_COOKIE (HWC_KEYBOARD == 2)

#define HWC_CONTROLLER_PVI (HWC_CONTROLLER == 0)
#define HWC_CONTROLLER_EPSON (HWC_CONTROLLER == 1 || HWC_CONTROLLER == 2)
#define HWC_CONTROLLER_DEPTH ((HWC_CONTROLLER > 0) ? 3 : 2)

#define HWC_SERIAL_RTS (HWC_PLATFORM == 0)
#define HWC_SERIAL_NOR2 (HWC_PLATFORM != 0)
#define HWC_PASSWORD_VIVI (HWC_PLATFORM == 0)
#define HWC_PASSWORD_NOR2 (HWC_PLATFORM != 0)

#define HWC_USE_WATCHDOG (HWC_PLATFORM != 0)
#define HWC_HAS_AUDIO (HWC_AUDIO != 0)
#define HWC_HAS_USB (HWC_USB != 0)
#define HWC_HAS_GSENSOR (HWC_GSENSOR != 0)
#define HWC_HAS_TOUCHPANEL (HWC_TOUCHPANEL != 0)
#define HWC_HAS_CAPTOUCH (HWC_CAPTOUCH != 0)
#define HWC_SUSPEND_CMD (HWC_PLATFORM == 0)
#define HWC_CANNOT_SHUTDOWN (HWC_PLATFORM == 1 || HWC_PLATFORM == 2)
#define HWC_MANUAL_WAVEFORM (HWC_CONTROLLER == 1 || HWC_CONTROLLER == 2)
#define HWC_EINK_PM_IOCTL (HWC_PLATFORM != 0)

int hw_init();
int hw_io_init();
int hw_eink_init();
int hw_gsensor_init();
int hw_touchpanel_init();
int hw_captouch_init();
void hw_close();
void hw_io_close();
void hw_eink_close();
void hw_gsensor_close();
void hw_touchpanel_close();
void hw_captouch_close();
int hw_safemode();
void hw_displaysize(int *w, int *h);
void hw_rotate(int n);
int hw_ioctl(int cmd, void *data);
void hw_setled(int state);
icanvas *hw_getframebuffer();
int hw_depth();
void hw_update(int x, int y, int w, int h, int bw);
void hw_fullupdate();
void hw_refine16();
int hw_capable16();
int hw_switch_dynamic(int n);
void hw_override_update(int mode);
void hw_suspend_display();
void hw_resume_display();
void hw_restore_screen();
int hw_useraction();
int hw_keystate(int key);
int hw_nextevent(int *type, int *par1, int *par2);
int hw_nextevent_key(int *type, int *par1, int *par2);
int hw_nextevent_ts(int *type, int *par1, int *par2);
int hw_nextevent_cap(int *type, int *par1, int *par2);
int hw_ts_active();
int hw_captouch_active();
int hw_isplaying();
int hw_ischarging();
int hw_isusbconnected();
int hw_isusbhostconnected();
int hw_isusbstorattached();
int hw_isusbmounted();
int hw_issdinserted();
void hw_usbhostpower(int n);
void hw_remountfs();
int hw_power();
int hw_temperature();
int hw_sleep(int ms, int deep);
time_t hw_gettime();
long long hw_timeinms();
void hw_setalarm(int ms);
void hw_say_poweroff();
int hw_shutting_down();
void hw_restart(int clearstate);
void hw_registerapp(pid_t pid);
char *hw_serialnumber();
char *hw_password();
unsigned int hw_pkey();
char *hw_waveform_filename();
char *hw_getmodel();
char *hw_getplatform();
char *hw_getmac();
int hw_writelogo(ibitmap *bm, int permanent);
int hw_restorelogo();
int hw_usbready();
void hw_usblink();
void hw_set_keylock(int en);
int hw_touchpanel_ready();
int hw_gsensor_ready();
int hw_read_gsensor(int *x, int *y, int *z);
int hw_last_gsensor();
void hw_set_gsensor(int mode);
void hw_suspend_gsensor();
void hw_resume_gsensor();
unsigned char *hw_calibrate_gsensor();
void hw_adjust_gsensor(unsigned char *data);
int hw_captouch_ready();
int hw_read_captouch();
int hw_flash_mounted();
void hw_winmessage(char *title, char *text, int flags);

int hw_iicreadb(int addr, int reg, int count, char *data);
int hw_iicwriteb(int addr, int reg, int count, char *data);
int hw_iicreadw(int addr, int reg, int count, short *data);
int hw_iicwritew(int addr, int reg, int count, short *data);

long hw_ipcrequest(long type, long param, unsigned char *data, int inlen, int outlen);

int hw_querynetwork();
char **hw_enum_btdevices();
char **hw_enum_wireless();
int hw_get_btservice(char *mac, char *service);
int hw_net_connect(char *name);
int hw_net_disconnect();
iv_netinfo *hw_net_info();

void hw_mp_loadplaylist(char **pl);
char **hw_mp_getplaylist();
void hw_mp_playtrack(int n);
int hw_mp_currenttrack();
int hw_mp_tracksize();
void hw_mp_settrackposition(int pos);
int hw_mp_trackposition();
void hw_mp_setstate(int state);
int hw_mp_getstate();
void hw_mp_setmode(int mode);
int hw_mp_getmode();
void hw_mp_setvolume(int n);
int hw_mp_getvolume();
void hw_mp_setequalizer(int *eq);
void hw_mp_getequalizer(int *eq);

const char *iv_evttype(int type);
iv_handler iv_seteventhandler(iv_handler hproc);
iv_handler iv_restoreeventhandler(iv_handler hproc, int keytm1, int keytm2);
void iv_enqueue(iv_handler hproc, int type, int par1, int par2);
char **iv_enum_files(char ***list, char *path1, char *path2, char *path3, char *extension);
void iv_drawsymbol(int x, int y, int size, int symbol, int color);
irect *iv_windowframe(int x, int y, int w, int h, int bordercolor, int windowcolor, char *title, int button);
void iv_scrollbar(int x, int y, int w, int h, int percent);
int iv_textblock(int x, int y, int w, unsigned short *p, int len, int color, int angle, int rtlbase);
void iv_stretch(const unsigned char *src, int format, int sw, int sh, int srclw,
		int dx, int dy, int dw, int dh, int flags, const unsigned char *mask, int masklw);
int iv_msgtop();
void def_openplayer();
bookinfo *def_getbookinfo(char *path);
ibitmap *def_getbookcover(char *path, int width, int height);
int iv_player_handler(int type, int par1, int par2);
void iv_setsoftupdate();
void iv_actualize_panel(int update);
void iv_update_panel(int with_time);
int iv_panelevent(int x, int y, int *type, int *par1, int *par2);
void iv_fullscreen();
void iv_update_orientation(int isexternal);
void iv_getkeymapping(char *act0[], char *act1[]);
void iv_rise_poweroff_timer();
void iv_key_timer();
void iv_poweroff_timer();
void iv_panelupdate_timer();
void iv_exit_handler();
void iv_setup_touchpanel();
void iv_setup_gsensor();
char *iv_shortpower_action(int times);
int iv_init_profiles();
int iv_use_profiles();
int iv_switch_profile(char *name);
void iv_check_profile();

int check_rtlbase(unsigned short *p, int len);
unsigned short *rtl_convert_l(unsigned short *s, int len, unsigned short **pos, int *back);
unsigned short *rtl_convert_r(unsigned short *s, int len);

#ifdef __cplusplus
}
#endif

#endif
