#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <uvgrtp/lib.hh>
#include <jpeglib.h>
#include <setjmp.h>


#define IMG_W 640
#define IMG_H 360
//define IMG_W 1280
//define IMG_H 720
//
uint32_t byte_counter = 0;

int port = 8888; 

// Display window size
int WINDOW_W = IMG_W;
int WINDOW_H = IMG_H;
 
int thread_exit = 0;
//
    // The width and height of the image
uint8_t tbuff[IMG_W * IMG_H * 3];

int rx_run(int argc, char* argv[]);

int camera_open(const char *dev);
int camera_capture(uint8_t **data, int *len);

struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */
    jmp_buf setjmp_buffer;        /* for return to caller */
};
typedef struct my_error_mgr *my_error_ptr;

void
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


int
refresh_video(void *opaque)
{
    int lcam = 0;
    const char* remote = (const char*)opaque;
    printf("Remote Device: %s\n", remote);

    uvg_rtp::context ctx;
    uvg_rtp::session *sess;
    uvg_rtp::media_stream *strm;

    // local camera capture
    int      cam_len;
    uint8_t *cam_data;

    if (remote[0] == '/') {
        // local camera device, not a RTP stream
        lcam = 1;
        camera_open(remote);
    } else {
        // rtp stream;
        sess = ctx.create_session(remote);
        strm = sess->create_stream(port, port, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);
        strm->configure_ctx(RCC_PKT_MAX_DELAY, 200);
    }

    int nframe = 0;
    for (;;) {
        uvgrtp::frame::rtp_frame *frame;

        if (lcam) {
            camera_capture(&cam_data, &cam_len);

        } else {
            frame = strm->pull_frame();
	    nframe++;
            //printf("got frame: %d, length=%d\n", nframe++, frame->payload_len);
        }

        int rc;
        // Variables for the decompressor itself
        struct jpeg_decompress_struct cinfo;
        //struct jpeg_error_mgr jerr;

        struct my_error_mgr jerr;

        // Variables for the output buffer, and how long each row is
        unsigned long bmp_size;
        unsigned char *bmp_buffer = NULL;
        int row_stride, width, height, pixel_size;

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = my_error_exit;

        // This is so confusing....
        if (setjmp(jerr.setjmp_buffer)) {
            goto cleanup;
        }

        jpeg_create_decompress(&cinfo);
        if (lcam) {
            jpeg_mem_src(&cinfo, cam_data, cam_len);
            byte_counter += (uint32_t)cam_len;
        } else {
            jpeg_mem_src(&cinfo, frame->payload, frame->payload_len);
            byte_counter += (uint32_t)frame->payload_len;
        }

        rc = jpeg_read_header(&cinfo, TRUE);
        if (rc != 1) {
            printf("not a normal JPEG blob\n");
            goto cleanup;
        }

        jpeg_start_decompress(&cinfo);

        width = cinfo.output_width;
        height = cinfo.output_height;
        pixel_size = cinfo.output_components;

        bmp_size = width * height * pixel_size;
        bmp_buffer = (unsigned char*) malloc(bmp_size);

        // The row_stride is the total number of bytes it takes to store an
        // entire scanline (row). 
        row_stride = width * pixel_size;

        while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char *buffer_array[1];
            buffer_array[0] = bmp_buffer + \
                           (cinfo.output_scanline) * row_stride;


            if (jpeg_read_scanlines(&cinfo, buffer_array, 1) != 1) {
                printf("did not read the scanline...\n");
            }
        }

        if (!jpeg_finish_decompress(&cinfo)) {
            printf("finish decompress didn't finish\n");
        }

        if (jerr.pub.num_warnings == 0) {
            // NOTE: This needs to be re-worked....
            memcpy(tbuff, bmp_buffer, sizeof(tbuff));

            SDL_Event event;
            event.type = SDL_USEREVENT + 1;
            SDL_PushEvent(&event);
        }
        
cleanup:
        jpeg_destroy_decompress(&cinfo);
        if (bmp_buffer)
            free(bmp_buffer);

        if (!lcam) {
            uvg_rtp::frame::dealloc_frame(frame);
        }
    }

    return 0;
}

Uint32
calculate_fps(Uint32 interval, void *param)
{
    int *framecounter = (int*)param;
    static Uint32 last = 0;
    Uint32 now = SDL_GetTicks();

    printf("elapsed = %u mS, fps: %2d, Mb/s: %1.1f\n", now - last, *framecounter, (float)((byte_counter) * 8) / 1000000.0);
    last = now;
    *framecounter = 0;
    byte_counter = 0;
    
    return interval;
}
 
int
main(int argc, char *argv[])
{
    int framecounter = 0;

    if (argc < 2) {
        printf("usage: \n");
        printf(" sdlplay [remote-ip|/dev/videoX]\n");
        return 1;
    }

    if (argc > 2) {
        printf("override port number: %s\n", argv[2]);
        port = atoi(argv[2]);
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("SDL Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
 
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, WINDOW_W, WINDOW_H);
    SDL_Thread *thread = SDL_CreateThread(refresh_video, NULL, argv[1]);
    SDL_AddTimer(1000, calculate_fps, &framecounter);
 
    SDL_Event event;
    SDL_Rect rect;

/* Image overlay test */
#if 0
    IMG_Init(IMG_INIT_PNG);
    SDL_Surface *battery_surface = IMG_Load("battery.png");
    if (battery_surface == NULL) {
        printf("Unable to load battery image\n");
        return 1;
    }
    SDL_Texture *btex = SDL_CreateTextureFromSurface(renderer, battery_surface);
    if (btex == NULL) {
        printf("Unable to create texture from surface\n");
        return 1;
    }
    SDL_FreeSurface(battery_surface); // no longer needed after texture creation
#endif
/* end overlay test */
 
    while (1) {
        SDL_WaitEvent(&event);
        if (event.type == SDL_USEREVENT + 1) {
            SDL_UpdateTexture(texture, NULL, tbuff, IMG_W * 3);
 
            rect.x = 0;
            rect.y = 0;
            rect.w = WINDOW_W;
            rect.h = WINDOW_H;
 
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &rect);
#if 0
            // Copy the battery icon to the screen
            rect.w = 32;
            rect.h = 32;
            SDL_RenderCopy(renderer, btex, NULL, &rect);

            rect.x = 32;
            SDL_RenderCopyEx(renderer, btex, NULL, &rect, 45, NULL, SDL_FLIP_NONE);
#endif
            SDL_RenderPresent(renderer);

            framecounter++;

        } else if(event.type == SDL_WINDOWEVENT) {
            SDL_GetWindowSize(window, &WINDOW_W, &WINDOW_H);
        } else if(event.type == SDL_QUIT) {
            break;
        } else if(event.type == SDL_KEYDOWN) {
            switch(event.key.keysym.sym) {
                case SDLK_UP:
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    break;
                case SDLK_DOWN:
                    SDL_SetWindowFullscreen(window, 0);
                    break;
                case SDLK_LEFT:
                    break;
                case SDLK_RIGHT:
                    break;
                default:
                    break;
            }
        }
    }
 
    return 0;
}

