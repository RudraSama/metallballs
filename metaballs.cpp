#include <xcb/xcb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <cmath>


typedef struct{
    uint8_t *pixels;
    uint8_t bytes_per_pixel;
    uint32_t bytes_per_row;
    uint32_t total_bytes;
} Pixel;

#define WIDTH 1200 
#define HEIGHT 800 

int ev_x = WIDTH/2;
int ev_y = HEIGHT/2;

static void fill_pixels(Pixel pixels, int width, int height, unsigned int color){
    //color's first 8 MSB are Blue, Middle 8 bits are Green and 8 LSB are Red
    uint8_t B = 0, G = 0, R = 0;
    R = R | color; 
    G = G | (color >> 8);
    B = B | (color >> 16);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int offset = y * pixels.bytes_per_row + x * pixels.bytes_per_pixel;
            pixels.pixels[offset + 0] = B; // Blue
            pixels.pixels[offset + 1] = G; // Green
            pixels.pixels[offset + 2] = R; // Red
        }
    }
}

static void fill_pixel(Pixel pixels, int x, int y, unsigned int color){
    if(x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT) return;

    uint8_t B = 0, G = 0, R = 0;
    R = R | color; 
    G = G | (color >> 8);
    B = B | (color >> 16);

    int offset = y * pixels.bytes_per_row + x * pixels.bytes_per_pixel;
    pixels.pixels[offset + 0] = B; // Blue
    pixels.pixels[offset + 1] = G; // Green
    pixels.pixels[offset + 2] = R; // Red
   
}


static void render(Pixel pixels){
    
    int cx = ev_x;
    int cy = ev_y;
    int r = 100;

    int cx2 = 250;
    int cy2 = 250;
    int r2 = 100;

    unsigned int color1 = 0x0000ff;
    unsigned int color2 = 0x00ff00;

    for(int y = 0; y <= HEIGHT; y++){
        for(int x = 0; x <= WIDTH; x++){
            float dx = cx - x;
            float dy = cy - y;
            float dx2 = cx2 - x;
            float dy2 = cy2 - y;
            float s = ((float)r/(dx*dx+dy*dy) + (float)r2/(dx2*dx2+dy2*dy2));

            uint8_t B1 = 0, G1 = 0, R1 = 0;
            R1 = R1 | color1; 
            G1 = G1 | (color1 >> 8);
            B1 = B1 | (color1 >> 16);

            uint8_t B2 = 0, G2 = 0, R2 = 0;
            R2 = R2 | color2; 
            G2 = G2 | (color2 >> 8);
            B2 = B2 | (color2 >> 16);

            int _dx = cx - cx2;
            int _dy = cy - cy2;

            float total_dist = sqrt((float)_dx*(float)_dx + (float)_dy*(float)_dy);
            float per = (sqrt(dx*dx + dy*dy)/total_dist)*100;

            unsigned int cR = R1 + per * (R2 - R1);
            unsigned int cG = G1 + per * (G2 - G1);
            unsigned int cB = B1 + per * (B2 - B1);

            unsigned color = 0x000000;

            color = color | cB;
            color = color << 8;
            color = color | cG;
            color = color << 8;
            color = color | cR;

            if(s >= 0.01){
                fill_pixel(pixels, x, y, color);
            }

        }
    }
 }

int main() {
    // Connect to X server
    xcb_connection_t *conn = xcb_connect(NULL, NULL);
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    // Create window
    xcb_window_t window = xcb_generate_id(conn);
    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK ;
    uint32_t values[] = { screen->black_pixel, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_POINTER_MOTION};

    xcb_create_window(
            conn, 
            screen->root_depth,
            window, 
            screen->root,
            0, 0,
            WIDTH, HEIGHT,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask, values);

    xcb_map_window(conn, window);

    xcb_flush(conn);

    // Create graphics context
    xcb_gcontext_t gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, window, 0, NULL);

    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, screen->root_depth,  pixmap, window, WIDTH, HEIGHT);


    // Determine bytes per pixel from screen->root_depth
    uint8_t bpp = 32; // Assume 32-bit color for simplicity
    uint8_t bytes_per_pixel = bpp / 8;
    uint32_t bytes_per_row = WIDTH * bytes_per_pixel;
    uint32_t total_bytes = bytes_per_row * HEIGHT;


    // Allocate and fill image buffer (Red color)
    uint8_t *buffer = (uint8_t*)calloc(1, total_bytes); 

    Pixel pixels;
    pixels.pixels = buffer;
    pixels.bytes_per_pixel = bytes_per_pixel;
    pixels.bytes_per_row = bytes_per_row;

    fill_pixels(pixels, WIDTH, HEIGHT, 0x000000);


    xcb_put_image(conn,
                  XCB_IMAGE_FORMAT_Z_PIXMAP,
                  pixmap,
                  gc,
                  WIDTH,
                  HEIGHT,
                  0, 0,   // x, y position
                  0,        // left_pad
                  screen->root_depth,
                  total_bytes,
                  pixels.pixels);



    xcb_flush(conn);




    while(true){
        // Wait for expose and draw the image
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(conn))) {
            switch (event->response_type & ~0x80) {
                case XCB_EXPOSE:{
                    break;
                }

                case XCB_KEY_PRESS:{
                    xcb_key_press_event_t *env = (xcb_key_press_event_t *)event;
                    if(env->detail == 9){
                        free(event);
                        goto end;
                    }
                    break;
               }
                case XCB_MOTION_NOTIFY:{
                    xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;
                    ev_x = ev->event_x;
                    ev_y = ev->event_y;
                    break;
               }
            }
            free(event);
        }

        fill_pixels(pixels, WIDTH, HEIGHT, 0x000000);
        render(pixels);
        xcb_put_image(conn,
                      XCB_IMAGE_FORMAT_Z_PIXMAP,
                      pixmap,
                      gc,
                      WIDTH,
                      HEIGHT,
                      0, 0,   // x, y position
                      0,        // left_pad
                      screen->root_depth,
                      total_bytes,
                      pixels.pixels);


        xcb_copy_area(conn, pixmap, window, gc, 0, 0, 0, 0, WIDTH, HEIGHT);

    }
end:
    xcb_disconnect(conn);
    return 0;
}

