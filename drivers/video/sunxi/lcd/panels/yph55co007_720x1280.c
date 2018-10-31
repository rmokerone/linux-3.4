#include "yph55co007_720x1280.h"
#include "panels.h"

#define FRAME_WIDTH  			(720)
#define FRAME_HEIGHT 			(1280)
#define LCM_OTM1287_ID			(0x1287)

static void LCD_power_on(__u32 sel);
static void LCD_power_off(__u32 sel);
static void LCD_bl_open(__u32 sel);
static void LCD_bl_close(__u32 sel);

static void LCD_panel_init(__u32 sel);
static void LCD_panel_exit(__u32 sel);

static void LCD_cfg_panel_info(panel_extend_para * info)
{
	__u32 i = 0, j=0;
	__u32 items;
	__u8 lcd_gamma_tbl[][2] =
	{
		//{input value, corrected value}
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	__u8 lcd_bright_curve_tbl[][2] =
	{
		//{input value, corrected value}
		{0    ,0  },//0
		{15   ,3  },//0
		{30   ,6  },//0
		{45   ,9  },// 1
		{60   ,12  },// 2
		{75   ,16  },// 5
		{90   ,22  },//9
		{105   ,28 }, //15
		{120  ,36 },//23
		{135  ,44 },//33
		{150  ,54 },
		{165  ,67 },
		{180  ,84 },
		{195  ,108},
		{210  ,137},
		{225 ,171},
		{240 ,210},
		{255 ,255},
	};

	__u32 lcd_cmap_tbl[2][3][4] = {
	{
		{LCD_CMAP_G0,LCD_CMAP_B1,LCD_CMAP_G2,LCD_CMAP_B3},
		{LCD_CMAP_B0,LCD_CMAP_R1,LCD_CMAP_B2,LCD_CMAP_R3},
		{LCD_CMAP_R0,LCD_CMAP_G1,LCD_CMAP_R2,LCD_CMAP_G3},
		},
		{
		{LCD_CMAP_B3,LCD_CMAP_G2,LCD_CMAP_B1,LCD_CMAP_G0},
		{LCD_CMAP_R3,LCD_CMAP_B2,LCD_CMAP_R1,LCD_CMAP_B0},
		{LCD_CMAP_G3,LCD_CMAP_R2,LCD_CMAP_G1,LCD_CMAP_R0},
		},
	};

	memset(info,0,sizeof(panel_extend_para));

	items = sizeof(lcd_gamma_tbl)/2;
	for(i=0; i<items-1; i++) {
		__u32 num = lcd_gamma_tbl[i+1][0] - lcd_gamma_tbl[i][0];

		for(j=0; j<num; j++) {
			__u32 value = 0;

			value = lcd_gamma_tbl[i][1] + ((lcd_gamma_tbl[i+1][1] - lcd_gamma_tbl[i][1]) * j)/num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] = (value<<16) + (value<<8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items-1][1]<<16) + (lcd_gamma_tbl[items-1][1]<<8) + lcd_gamma_tbl[items-1][1];

	items = sizeof(lcd_bright_curve_tbl)/2;
	for(i=0; i<items-1; i++) {
		__u32 num = lcd_bright_curve_tbl[i+1][0] - lcd_bright_curve_tbl[i][0];

		for(j=0; j<num; j++) {
			__u32 value = 0;

			value = lcd_bright_curve_tbl[i][1] + ((lcd_bright_curve_tbl[i+1][1] - lcd_bright_curve_tbl[i][1]) * j)/num;
			info->lcd_bright_curve_tbl[lcd_bright_curve_tbl[i][0] + j] = value;
		}
	}
	info->lcd_bright_curve_tbl[255] = lcd_bright_curve_tbl[items-1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static __s32 LCD_open_flow(__u32 sel)
{
	LCD_OPEN_FUNC(sel, LCD_power_on, 100);   //open lcd power, and delay 50ms
	LCD_OPEN_FUNC(sel, LCD_panel_init, 200);   //open lcd power, than delay 200ms
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);     //open lcd controller, and delay 100ms
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);     //open lcd backlight, and delay 0ms

	return 0;
}

static __s32 LCD_close_flow(__u32 sel)
{
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);       //close lcd backlight, and delay 0ms
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);         //close lcd controller, and delay 0ms
	LCD_CLOSE_FUNC(sel, LCD_panel_exit,	200);   //open lcd power, than delay 200ms
	LCD_CLOSE_FUNC(sel, LCD_power_off, 50);   //close lcd power, and delay 500ms

	return 0;
}

static void LCD_power_on(__u32 sel)
{
	sunxi_lcd_power_enable(sel, 0);//config lcd_power pin to open lcd power0
	sunxi_lcd_power_enable(sel, 1);//config lcd_power pin to open lcd power1
	sunxi_lcd_power_enable(sel, 2);//config lcd_power pin to open lcd power2
	sunxi_lcd_pin_cfg(sel, 1);
}

static void LCD_power_off(__u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_power_disable(sel, 0);//config lcd_power pin to close lcd power0
	sunxi_lcd_power_disable(sel, 1);//config lcd_power pin to close lcd power1
	sunxi_lcd_power_disable(sel, 2);//config lcd_power pin to close lcd power2
}

static void LCD_bl_open(__u32 sel)
{
	sunxi_lcd_pwm_enable(sel);//open pwm module
	sunxi_lcd_backlight_enable(sel);//config lcd_bl_en pin to open lcd backlight
}

static void LCD_bl_close(__u32 sel)
{
	sunxi_lcd_backlight_disable(sel);//config lcd_bl_en pin to close lcd backlight
	sunxi_lcd_pwm_disable(sel);//close pwm module
}

#define REGFLAG_DELAY             0X01
#define REGFLAG_END_OF_TABLE      0x02   // END OF REGISTERS MARKER

struct LCM_setting_table {
    __u8 cmd;
    __u32 count;
    __u8 para_list[64];
};

#if 0
static struct LCM_setting_table otm1283_test_config_para[] = {
	{0x00, 1, 0X00},
	{0xff, 3, {0x12, 0x83, 0x01}},
	{0x00, 1, 0x80},
	{0xff, 2 , {0x12, 0x83}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}	
};
#endif

static struct LCM_setting_table otm1283_initialization_setting[] = {

    {0x00,1,{0x00}},
	{0xff,3,{0x12,0x87,0x01}},	//EXTC=1
	{0x00,1,{0x80}},	        //Orise mode enable
	{0xff,2,{0x12,0x87}},

	{0x00,1,{0x92}},				
	{0xff,2,{0x30,0x02}},		//MIPI 4 Lane

    //-------------------- panel setting --------------------//
	{0x00,1,{0x80}},             //TCON Setting
	{0xc0,9,{0x00,0x64,0x00,0x10,0x10,0x00,0x64,0x10,0x10}},

	{0x00,1,{0x90}},             //Panel Timing Setting
	{0xc0,6,{0x00,0x5c,0x00,0x01,0x00,0x04}},

	{0x00,1,{0xb3}},             //Interval Scan Frame: 0 frame, column inversion
	{0xc0,2,{0x00,0x55}},

	{0x00,1,{0x81}},             //frame rate:60Hz
	{0xc1,1,{0x55}},

	//-------------------- power setting --------------------//
	{0x00,1,{0xa0}},             //dcdc setting
	{0xc4,14,{0x05,0x10,0x04,0x02,0x05,0x15,0x11,0x05,0x10,0x07,0x02,0x05,0x15,0x11}},

	{0x00,1,{0xb0}},             //clamp voltage setting
	{0xc4,2,{0x00,0x00}},

	{0x00,1,{0x91}},             //VGH=16V, VGL=-12V, pump ratio:VGH=8x, VGL=-5x
	{0xc5,2,{0x69,0xd2}},
	
	{0x00,1,{0x00}},             //GVDD=5.008V, NGVDD=-5.008V
	{0xd8,2,{0xa7,0xa7}},    //c7 
	
	{0x00,1,{0x00}},             //VCOM=-1.63V
	{0xd9,1,{0x78}},             
	
	{0x00,1,{0xb3}},             //VDD_18V=1.7V, LVDSVDD=1.6V
	{0xc5,1,{0x84}},
	
	{0x00,1,{0xbb}},             //LVD voltage level setting
	{0xc5,1,{0x8a}},
	
	{0x00,1,{0x82}},		//chopper
	{0xC4,1,{0x0a}},
	
	{0x00,1,{0xc6}},		//debounce
	{0xb0,1,{0x03}},

    //-------------------- control setting --------------------//
	{0x00,1,{0x00}},             //ID1
	{0xd0,1,{0x40}},
	
	{0x00,1,{0x00}},             //ID2, ID3
	{0xd1,2,{0x00,0x00}},

    //-------------------- power on setting --------------------//
	{0x00,1,{0xb2}},             //VGLO1
	{0xf5,2,{0x00,0x00}},
	
	{0x00,1,{0xb6}},             //VGLO2
	{0xf5,2,{0x00,0x00}},
	
	{0x00,1,{0x94}},             //VCL pump dis
	{0xf5,2,{0x00,0x00}},
	
	{0x00,1,{0xd2}},             //VCL reg. en
	{0xf5,2,{0x06,0x15}},
	
	{0x00,1,{0xb4}},             //VGLO1/2 Pull low setting
	{0xc5,1,{0xcc}},	       //d[7] vglo1 d[6] vglo2 => 0: pull vss, 1: pull vgl

    //-------------------- for Power IC ---------------------------------
	{0x00,1,{0x90}},             //Mode-3
	{0xf5,4,{0x02,0x11,0x02,0x15}},
	
	{0x00,1,{0x90}},             //2xVPNL, 1.5*=00, 2*=50, 3*=a0
	{0xc5,1,{0x50}},
	
	{0x00,1,{0x94}},             //Frequency
	{0xc5,1,{0x66}},

    //-------------------- panel timing state control --------------------//
	{0x00,1,{0x80}},             //panel timing state control
	{0xcb,11,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0x90}},             //panel timing state control
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xa0}},             //panel timing state control
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xb0}},             //panel timing state control
	{0xcb,15,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xc0}},             //panel timing state control
	{0xcb,15,{0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x00,0x05,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xd0}},             //panel timing state control
	{0xcb,15,{0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05}},
	
	{0x00,1,{0xe0}},             //panel timing state control
	{0xcb,14,{0x05,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x00}},
	
	{0x00,1,{0xf0}},             //panel timing state control
	{0xcb,11,{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},

    //-------------------- panel pad mapping control --------------------//
	{0x00,1,{0x80}},             //panel pad mapping control
	{0xcc,15,{0x29,0x2a,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x06,0x00,0x08,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0x90}},             //panel pad mapping control
	{0xcc,15,{0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x29,0x2a,0x09,0x0b,0x0d,0x0f,0x11,0x13}},
	
	{0x00,1,{0xa0}},             //panel pad mapping control
	{0xcc,14,{0x05,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00}},
	
	{0x00,1,{0xb0}},             //panel pad mapping control
	{0xcc,15,{0x29,0x2a,0x13,0x11,0x0f,0x0d,0x0b,0x09,0x01,0x00,0x07,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xc0}},             //panel pad mapping control
	{0xcc,15,{0x00,0x00,0x00,0x00,0x05,0x00,0x00,0x29,0x2a,0x14,0x12,0x10,0x0e,0x0c,0x0a}},
	
	{0x00,1,{0xd0}},             //panel pad mapping control
	{0xcc14,{0x02,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00}},

    //-------------------- panel timing setting --------------------//
	{0x00,1,{0x80}},             //panel VST setting
	{0xce,12,{0x89,0x05,0x00,0x88,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0x90}},             //panel VEND setting
	{0xce,14,{0x54,0xfc,0x00,0x54,0xfd,0x00,0x55,0x00,0x00,0x55,0x01,0x00,0x00,0x00}},
	
	{0x00,1,{0xa0}},             //panel CLKA1/2 setting
	{0xce,14,{0x58,0x07,0x05,0x08,0x00,0x00,0x00,0x58,0x06,0x05,0x09,0x00,0x00,0x00}},
	
	{0x00,1,{0xb0}},             //panel CLKA3/4 setting
	{0xce,14,{0x58,0x05,0x05,0x0A,0x00,0x00,0x00,0x58,0x04,0x05,0x0B,0x00,0x00,0x00}},
	
	{0x00,1,{0xc0}},             //panel CLKb1/2 setting
	{0xce,14,{0x58,0x03,0x05,0x0C,0x00,0x00,0x00,0x58,0x02,0x05,0x0D,0x00,0x00,0x00}},
	
	{0x00,1,{0xd0}},             //panel CLKb3/4 setting
	{0xce,14,{0x58,0x01,0x05,0x0E,0x00,0x00,0x00,0x58,0x00,0x05,0x0F,0x00,0x00,0x00}},
	
	{0x00,1,{0x80}},             //panel CLKc1/2 setting
	{0xcf,14,{0x50,0x00,0x05,0x10,0x00,0x00,0x00,0x50,0x01,0x05,0x11,0x00,0x00,0x00}},
	
	{0x00,1,{0x90}},             //panel CLKc3/4 setting
	{0xcf,14,{0x50,0x02,0x05,0x12,0x00,0x00,0x00,0x50,0x03,0x05,0x13,0x00,0x00,0x00}},
	
	{0x00,1,{0xa0}},             //panel CLKd1/2 setting
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xb0}},             //panel CLKd3/4 setting
	{0xcf,14,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	
	{0x00,1,{0xc0}},             //panel ECLK setting
	{0xcf,11,{0x3D,0x3D,0x20,0x20,0x00,0x00,0x01,0x01,0x20,0x00,0x00}},
	
	{0x00,1,{0xb5}},
	{0xc5,6,{0x0b,0x95,0xff,0x0b,0x95,0xff}},
	
	{0x00,1,{0x00}},                                                                                   ////
	{0xE1,20,{0x15,0x38,0x46,0x52,0x62,0x6E,0x6f,0x94,0x81,0x97,0x6f,0x5b,0x6e,0x4a,0x47,0x38,0x27,0x16,0x08,0x05}},
	
	{0x00,1,{0x00}},
	{0xE2,20,{0x15,0x38,0x46,0x52,0x62,0x6E,0x6f,0x94,0x81,0x97,0x6f,0x5b,0x6e,0x4a,0x47,0x38,0x27,0x16,0x08,0x05}},
	
	{0x00,1,{0x00}},             //Orise mode disable				
	{0xff,3,{0xff,0xff,0xff}},
	
	{0x36,1,{0x00}}, 
	 
	{0x35,1,{0x00}},//TE
	
	{0x11,1,{0x00}},    
	 {REGFLAG_DELAY,120,{}},                                
	{0x29,1,{0x00}},//Display ON 

	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}	
};



static struct LCM_setting_table lcm_sleep_out_setting[] = {
	// Sleep Out
	{0x11, 0, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	// Display ON
	{0x29, 0, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table lcm_sleep_in_setting[] = {
	// Display off sequence
	{0x28, 0, {0x00}},
	{REGFLAG_DELAY, 50, {}},
	// Sleep Mode On
	{0x10, 0, {0x00}},
	{REGFLAG_DELAY, 100, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};


static struct LCM_setting_table hx8394d_test_config_para[] = {
	{0xB9, 3, {0xFF, 0x83, 0x94}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}	
};


static struct LCM_setting_table hx8394d_initialization_setting[] = {
#if 0	
	{0xB9,3,{0XFF,0X83,0X94}},
	{0xBA,2,{0X33,0X83}},
	{0xB0,4,{0X00,0X00,0X7D,0X0C}},
	{0xB1,15,{0X6C,0X15,0X15,0X24,0X04,0X11,0XF1,0X80,0XE4,0X97,0X23,0X80,0XC0,0XD2,0X58}},
	{0xB2,11,{0X00,0X64,0X10,0X07,0X22,0X1C,0X08,0X08,0X1C,0X4D,0X00}},
	{0xB4,12,{0X00,0XFF,0X03,0X70,0X03,0X70,0X03,0X70,0X01,0X74,0X01,0X74}},
	{0xBF,3,{0X41,0X0E,0X01}},
	{0xD3,37,{0X00,0X06,0X00,0X40,0X07,0X08,0X00,0X32,0X10,0X07,0X00,0X07,0X54,0X15,0X0F,0X05,0X04,0X02,0X12,0X10,0X05,0X07,0X33,0X33,0X0B,0X0B,0X37,0X10,0X07,0X07,0X08,0X00,0X00,0X00,0X0A,0X00,0X01}},
	{0xD5,44,{0X04,0X05,0X06,0X07,0X00,0X01,0X02,0X03,0X20,0X21,0X22,0X23,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X19,0X19,0X18,0X18,0X18,0X18,0X1B,0X1B,0X1A,0X1A,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18}},
	{0xD6,44,{0X03,0X02,0X01,0X00,0X07,0X06,0X05,0X04,0X23,0X22,0X21,0X20,0X18,0X18,0X18,0X18,0X18,0X18,0X58,0X58,0X18,0X18,0X19,0X19,0X18,0X18,0X1B,0X1B,0X1A,0X1A,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18}},
	{0xCC,1,{0X09}},
	{0xB6,2,{0X4C,0X4C}},
	{0xE0,42,{0X00,0X10,0X16,0X2D,0X33,0X3F,0X23,0X3E,0X07,0X0B,0X0D,0X17,0X0E,0X12,0X14,0X12,0X13,0X06,0X11,0X13,0X18,0X00,0X0F,0X16,0X2E,0X33,0X3F,0X23,0X3D,0X07,0X0B,0X0D,0X18,0X0F,0X12,0X14,0X12,0X14,0X07,0X11,0X12,0X17}},
	{0xC7,4,{0X00,0XC0,0X00,0XC0}},
	{0x11,0,{}},
	{REGFLAG_DELAY, 150, {}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}		
#else //debug only
	{0xB9,3,{0XFF,0X83,0X94}},
	{0xBA,2,{0X33,0X83}},
	{0xB0,4,{0X00,0X00,0X7D,0X0C}},
	{0xB1,15,{0X6C,0X15,0X15,0X24,0X04,0X11,0XF1,0X80,0XE4,0X97,0X23,0X80,0XC0,0XD2,0X58}},
	{0xB2,11,{0X00,0X64,0X10,0X07,0X22,0X1C,0X08,0X08,0X1C,0X4D,0X00}},
	{0xB4,12,{0X00,0XFF,0X03,0X5a,0X03,0X5a,0X03,0X5a,0X01,0X6a,0X30,0X6a}},
	{0xBc,1,{0X07}},
	{0xBF,3,{0X41,0X0E,0X01}},
	{0xB6,2,{0X5C,0X5C}},
	{0xCC,1,{0X09}},
	{0xD3,30,{0X00,0X06,0X00,0X40,0X07,0X08,0X00,0X32,0X10,0X07,0X00,0X07,0X54,0X15,0X0F,0X05,0X04,0X02,0X12,0X10,0X05,0X07,0X33,0X33,0X0B,0X0B,0X37,0X10,0X07,0X07}},
	{0xD5,44,{0X04,0X05,0X06,0X07,0X00,0X01,0X02,0X03,0X20,0X21,0X22,0X23,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X19,0X19,0X18,0X18,0X18,0X18,0X1B,0X1B,0X1A,0X1A,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18}},
	{0xD6,44,{0X03,0X02,0X01,0X00,0X07,0X06,0X05,0X04,0X23,0X22,0X21,0X20,0X18,0X18,0X18,0X18,0X18,0X18,0X58,0X58,0X18,0X18,0X19,0X19,0X18,0X18,0X1B,0X1B,0X1A,0X1A,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18,0X18}},
	{0xE0,42,{0X00,0X10,0X16,0X2D,0X33,0X3F,0X23,0X3E,0X07,0X0B,0X0D,0X17,0X0E,0X12,0X14,0X12,0X13,0X06,0X11,0X13,0X18,0X00,0X0F,0X16,0X2E,0X33,0X3F,0X23,0X3D,0X07,0X0B,0X0D,0X18,0X0F,0X12,0X14,0X12,0X14,0X07,0X11,0X12,0X17}},
	{0xc0,2,{0X30,0X14}},
	{0xC7,4,{0X00,0XC0,0X00,0XC0}},
	{0xdf,1,{0X8e}},
	{0xd2,1,{0X66}},
	{0x11,0,{}},
	{REGFLAG_DELAY, 500, {}},
	{0x29,0,{}},
	{REGFLAG_DELAY, 50, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}} 
#endif
};


static void LCD_panel_init(__u32 sel)
{
	__u32 i;
	__u32 rx_num ;
	__u8 rx_bf0,rx_bf1,rx_bf2;
	__u32 hx8394d_used=0;

	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_power_disable(sel, 0);//config lcd_power pin to close lcd power0
	sunxi_lcd_dsi_clk_disable(1);
	sunxi_lcd_delay_ms(40);//Wait more than 40ms

	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_delay_ms(10);

	sunxi_lcd_dsi_clk_enable(1);

	sunxi_lcd_delay_ms(10);

	panel_rst(1);	 //add by lyp@20140423
	sunxi_lcd_delay_ms(10);//add by lyp@20140423
	panel_rst(0);
	sunxi_lcd_delay_ms(20);
	panel_rst(1);
	sunxi_lcd_delay_ms(120);


	for (i=0;;i++) 
	{
		if(otm1283_initialization_setting[i].cmd == 0x02)
			break;
		else if (otm1283_initialization_setting[i].cmd == 0x01)
			sunxi_lcd_delay_ms(otm1283_initialization_setting[i].count);
		else
			dsi_dcs_wr(0,otm1283_initialization_setting[i].cmd,otm1283_initialization_setting[i].para_list,otm1283_initialization_setting[i].count);		
	}

	//add by gongjun
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_dsi_clk_enable(sel);

	dsi_dcs_wr_0para(sel,0x11); 		//¹š¿¡ÐÞžÄ
	sunxi_lcd_delay_ms(100);//Wait more than 100ms

	dsi_dcs_wr_0para(sel,0x29); //Display on		¹š¿¡ÐÞžÄ
//	sunxi_lcd_delay_ms(200);
	
	return;
}

static void LCD_panel_exit(__u32 sel)
{
	sunxi_lcd_dsi_clk_disable(sel);
	panel_rst(0);
	return ;
}

//sel: 0:lcd0; 1:lcd1
static __s32 LCD_user_defined_func(__u32 sel, __u32 para1, __u32 para2, __u32 para3)
{
	return 0;
}

__lcd_panel_t yph55co007_720x1280_panel = {
	/* panel driver name, must mach the name of lcd_drv_name in sys_config.fex */
	.name = "yph55co007",
	.func = {
		.cfg_panel_info = LCD_cfg_panel_info,
		.cfg_open_flow = LCD_open_flow,
		.cfg_close_flow = LCD_close_flow,
		.lcd_user_defined_func = LCD_user_defined_func,
	},
};
