#include "panels.h"
#include "default_panel.h"
#include "tft720x1280.h"
#include "starry768x1024.h"
#include "xbw080al02_800x1280.h"
#include "yx550dpaoa002_720x1280.h"
#include "yph55co007_720x1280.h"

extern __lcd_panel_t tft720x1280_panel;
extern __lcd_panel_t vvx10f004b00_panel;
extern __lcd_panel_t lp907qx_panel;
extern __lcd_panel_t sl698ph_720p_panel;
extern __lcd_panel_t lp079x01_panel;
extern __lcd_panel_t xbw080al02_800x1280_panel;
extern __lcd_panel_t yx550dpaoa002_720x1280_panel;
extern __lcd_panel_t yph55co007_720x1280_panel;

__lcd_panel_t* panel_array[] = {
	&default_panel,
	&tft720x1280_panel,
	&vvx10f004b00_panel,
	&lp907qx_panel,
	&starry768x1024_panel,
	&sl698ph_720p_panel,
	&lp079x01_panel,
	&xbw080al02_800x1280_panel,
	&yx550dpaoa002_720x1280_panel,
        &yph55co007_720x1280_panel,
	/* add new panel below */

	NULL,
};

