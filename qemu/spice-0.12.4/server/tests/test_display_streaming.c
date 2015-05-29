/* Do repeated updates to the same rectangle to trigger stream creation.
 *
 * TODO: check that stream actually starts programatically (maybe stap?)
 * TODO: stop updating same rect, check (prog) that stream stops
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "test_display_base.h"

static int sized;
static int render_last_frame;

static void create_overlay(Command *command , int width, int height)
{
    CommandDrawBitmap *cmd = &command->bitmap;
    uint32_t *dst;

    cmd->surface_id = 0;

    cmd->bbox.left = 0;
    cmd->bbox.top = 0;
    cmd->bbox.right = width;
    cmd->bbox.bottom = height;

    cmd->num_clip_rects = 0;
    cmd->bitmap = malloc(width * height * 4 );
    dst = (uint32_t *)cmd->bitmap;
    for (int i = 0; i < width * height; i++, dst++) {
        *dst = 0x8B008B;
    }

}

#define NUM_COMMANDS 2000
#define SIZED_INTERVAL 100
#define OVERLAY_FRAME 500
#define OVERLAY_WIDTH 200
#define OVERLAY_HEIGHT 200

/*
 * Create a frame in a stream that displays a row that moves
 * from the top to the bottom repeatedly.
 * Upon the OVERLAY_FRAME-th, a drawable is created on top of a part of the stream,
 * and from then on, all the stream frames has a clipping that keeps this drawable
 * visible, and in addition a clipping_factor is subtracted from the right limit of their clipping.
 * If sized=TRUE, a higher and wider frame than the original stream is created every SIZED_INTERVAL.
 * The sized frames can be distinguished by a change in the color of the top and bottom limits of the
 * surface.
 */
static void create_clipped_frame(Test *test, Command *command, int clipping_factor)
{
    static int count = 0;
    CommandDrawBitmap *cmd = &command->bitmap;
    int max_height = test->height;
    int max_width = test->width;
    int width;
    int height;
    int cur_line, end_line;
    uint32_t *dst;

    count++;
    if (count == NUM_COMMANDS) {
        count = 0;
    }
    if (count == OVERLAY_FRAME) {
        create_overlay(command, OVERLAY_WIDTH, OVERLAY_HEIGHT);
        return;
    }

    cmd->surface_id = 0;

    cmd->bbox.left = 0;
    cmd->bbox.right = max_width - 50;
    assert(max_height > 600);
    cmd->bbox.top = 50;
    cmd->bbox.bottom = max_height - 50;
    height = cmd->bbox.bottom  - cmd->bbox.top;
    width = cmd->bbox.right - cmd->bbox.left;
    cur_line = (height/30)*(count % 30);
    end_line = cur_line + (height/30);
    if (end_line >= height || height - end_line < 8) {
        end_line = height;
    }

    if (sized && count % SIZED_INTERVAL == 0) {

        cmd->bbox.top = 0;
        cmd->bbox.bottom = max_height;
        cmd->bbox.left = 0;
        cmd->bbox.right = max_width;
        height = max_height;
        width = max_width;
        cur_line += 50;
        end_line += 50;
    }

    cmd->bitmap = malloc(width*height*4);
    memset(cmd->bitmap, 0xff, width*height*4);
    dst = (uint32_t *)(cmd->bitmap + cur_line*width*4);
    for (cur_line; cur_line < end_line; cur_line++) {
        int col;
        for (col = 0; col < width; col++, dst++) {
            *dst = 0x00FF00;
        }
    }
    if (sized && count % SIZED_INTERVAL == 0) {
        int i;
        uint32_t color = 0xffffff & rand();

        dst = (uint32_t *)cmd->bitmap;

        for (i = 0; i < 50*width; i++, dst++) {
            *dst = color;
        }

        dst = ((uint32_t *)(cmd->bitmap + (height - 50)*4*width));

        for (i = 0; i < 50*width; i++, dst++) {
            *dst = color;
        }
    }

    if (count < OVERLAY_FRAME) {
        cmd->num_clip_rects = 0;
    } else {
        cmd->num_clip_rects = 2;
        cmd->clip_rects = calloc(sizeof(QXLRect), 2);
        cmd->clip_rects[0].left = OVERLAY_WIDTH;
        cmd->clip_rects[0].top = cmd->bbox.top;
        cmd->clip_rects[0].right = cmd->bbox.right - clipping_factor;
        cmd->clip_rects[0].bottom = OVERLAY_HEIGHT;
        cmd->clip_rects[1].left = cmd->bbox.left;
        cmd->clip_rects[1].top = OVERLAY_HEIGHT;
        cmd->clip_rects[1].right = cmd->bbox.right - clipping_factor;
        cmd->clip_rects[1].bottom = cmd->bbox.bottom;
    }
}

static void create_frame1(Test *test, Command *command)
{
    create_clipped_frame(test, command, 0);
}

void create_frame2(Test *test, Command *command)
{
    create_clipped_frame(test, command, 200);
}

typedef void (*create_frame_cb)(Test *test, Command *command);


/*
 * The test contains two types of streams. The first stream doesn't
 * have a clipping besides the on that the display the overlay drawable.
 * Expected result: If render_last_frame=false, the last frame should
 * be sent losslessly. Otherwise, red_update_area should be called, and the
 * stream is upgraded by a screenshot.
 *
 * In the second test, the stream clip changes in the middle (becomes smaller).
 * Expected result: red_update_area should is, and the
 * stream is upgraded by a screenshot (including lossy areas that belong to old frames
 * and were never covered by a lossless drawable).
 *
 */
static void get_stream_commands(Command *commands, int num_commands,
                                create_frame_cb cb)
{
    int i;

    commands[0].command = DESTROY_PRIMARY;
    commands[1].command = CREATE_PRIMARY;
    commands[1].create_primary.width = 1280;
    commands[1].create_primary.height = 1024;
    commands[num_commands - 1].command = SLEEP;
    commands[num_commands - 1].sleep.secs = 20;

    for (i = 2; i < num_commands - 1; i++) {
        commands[i].command = SIMPLE_DRAW_BITMAP;
        commands[i].cb = cb;
    }
    if (render_last_frame) {
        commands[num_commands - 2].command = SIMPLE_UPDATE;
    }
}

static void get_commands(Command **commands, int *num_commands)
{
    *num_commands = NUM_COMMANDS * 2;
    *commands = calloc(sizeof(Command), *num_commands);

    get_stream_commands(*commands, NUM_COMMANDS, create_frame1);
    get_stream_commands((*commands) + NUM_COMMANDS, NUM_COMMANDS, create_frame2);
}


int main(int argc, char **argv)
{
    SpiceCoreInterface *core;
    Command *commands;
    int num_commands;
    int i;
    Test *test;

    spice_test_config_parse_args(argc, argv);
    sized = 0;
    for (i = 1 ; i < argc; ++i) {
        if (strcmp(argv[i], "sized") == 0) {
            sized = 1;
        }
        /* render last frame */
        if (strcmp(argv[i], "render") == 0) {
            render_last_frame = 1;
        }
    }
    srand(time(NULL));
    // todo: add args list of test numbers with explenations
    core = basic_event_loop_init();
    test = test_new(core);
    spice_server_set_streaming_video(test->server, SPICE_STREAM_VIDEO_ALL);
    test_add_display_interface(test);
    get_commands(&commands, &num_commands);
    test_set_command_list(test, commands, num_commands);
    basic_event_loop_mainloop();
    free(commands);
    return 0;
}
