#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define DISP_BUF_SIZE (128 * 1024)

static lv_obj_t * chart;
lv_coord_t ecg_sample[3];

lv_chart_series_t *ser;

void update_chart(lv_timer_t *timer);

int client_fd, status, valread;
char buffer[1024];

int main(int argc, char *argv[]) {
	struct sockaddr_in serv_addr;
	int opt = 1;

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5003);

	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

	status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	if(argc != 5) {
		printf("Usage: %s width height fbdev_path evdev_path\n", argv[0]);
		return 1;
	}

	/*LittlevGL init*/
	lv_init();

	/*Linux frame buffer device init*/
	fbdev_init(argv[3]);

	/*A small buffer for LittlevGL to draw the screen's content*/
	static lv_color_t buf[DISP_BUF_SIZE];

	/*Initialize a descriptor for the buffer*/
	static lv_disp_draw_buf_t disp_buf;
	lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

	/*Initialize and register a display driver*/
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.draw_buf   = &disp_buf;
	disp_drv.flush_cb   = fbdev_flush;
	disp_drv.hor_res	= atoi(argv[1]);
	disp_drv.ver_res	= atoi(argv[2]);
	lv_disp_drv_register(&disp_drv);

	evdev_init(argv[4]);
	static lv_indev_drv_t indev_drv_1;
	lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
	indev_drv_1.type = LV_INDEV_TYPE_POINTER;

	/*This function will be called periodically (by the library) to get the mouse position and state*/
	indev_drv_1.read_cb = evdev_read;
	lv_indev_t *mouse_indev = lv_indev_drv_register(&indev_drv_1);


	/*Set a cursor for the mouse*/
	LV_IMG_DECLARE(mouse_cursor_icon)
	lv_obj_t * cursor_obj = lv_img_create(lv_scr_act()); /*Create an image object for the cursor */
	lv_img_set_src(cursor_obj, &mouse_cursor_icon);		   /*Set the image source*/
	lv_indev_set_cursor(mouse_indev, cursor_obj);			 /*Connect the image  object to the driver*/

	chart = lv_chart_create(lv_scr_act());
	lv_obj_set_size(chart, disp_drv.hor_res, disp_drv.ver_res);
	lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
	lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
	lv_chart_set_point_count(chart, 1000);
	ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

	lv_timer_t *timer = lv_timer_create(update_chart, 100, 0);


	/*Handle LitlevGL tasks (tickless mode)*/

	while(1) {
		lv_timer_handler();
		usleep(5000);
	}

	return 0;
}


void update_chart(lv_timer_t *timer) {
	valread = read(client_fd, buffer, 1024);
	int a = 0, b = 0, k = 0;
	char p[100];
	for(int j = 0, f = 0; j < 1024; j++) {
		if(buffer[j] == '|') {
			if(f) {
				b = j;
				break;
			} else {
				a = j, f = 1;
				continue;
			}
		}
		if(f) p[k] = buffer[j], k += 1;
	}
	p[k] = 0;
	if(atof(p) == 0) return;
	lv_chart_set_next_value(chart, ser, atof(p));
	lv_chart_refresh(chart);
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
	static uint64_t start_ms = 0;
	if(start_ms == 0) {
		struct timeval tv_start;
		gettimeofday(&tv_start, NULL);
		start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
	}

	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	uint64_t now_ms;
	now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

	uint32_t time_ms = now_ms - start_ms;
	return time_ms;
}
