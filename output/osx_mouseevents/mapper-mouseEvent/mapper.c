//
//  mapper.c
//  mapper-mouseEvent
//
//  Created by malloch on 08/04/2019.
//  Copyright © 2019 Graphics and Experiential Media Lab. All rights reserved.
//

#include "mapper.h"

extern void emit_mouse_evt(int, float, float, int);

mpr_dev dev = NULL;

int updated;
float epsilon = 0.001;

int leftButtonStatus = 0;
int rightButtonStatus = 0;

typedef struct {
    float x;
    float y;
} point;

point current;
point last;

void errorHandler(int num, const char *msg, const char *where)
{
    printf("liblo server error %d in path %s: %s\n", num, where, msg);
}

int sign(int value)
{
    return value > 0 ? 1 : -1;
}

double get_current_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1000000.0;
}

// grab time when button down pressed, if within 1sec increment click count
int click_count(int evt)
{
    static int click_counter = 1;
    static double timetag = 0;

    if (evt == LEFT_BUTTON_DOWN || evt == RIGHT_BUTTON_DOWN) {
        double now = get_current_time();
        if ((now - timetag) < 1.0)
            click_counter = 2;
        else
            click_counter = 1;
        timetag = now;
    }
    return click_counter;
}

int emit()
{
    float dx = current.x - last.x;
    float dy = current.y - last.y;

    // handle buttons
    int evt = 0;
    if (leftButtonStatus > 1)
        evt = LEFT_BUTTON_DOWN;
    else if (leftButtonStatus < -1)
        evt = LEFT_BUTTON_UP;
    else if (rightButtonStatus > 1)
        evt = RIGHT_BUTTON_DOWN;
    else if (rightButtonStatus < -1)
        evt = RIGHT_BUTTON_UP;

    if (evt) {
        int count = click_count(evt);
        printf("emitting CLICK %d x %d\n", evt, count);
        emit_mouse_evt(evt, current.x, current.y, count);
        leftButtonStatus = sign(leftButtonStatus);
        rightButtonStatus = sign(rightButtonStatus);
        return 0;
    }

    if (fabsf(dx) < epsilon && fabsf(dy) < epsilon)
        return 0;

    last.x = current.x;
    last.y = current.y;

    if (leftButtonStatus > 0) {
        // left mouse drag
        printf("emitting LEFT_MOUSE_DRAGGED\n");
        emit_mouse_evt(LEFT_MOUSE_DRAGGED, current.x, current.y, 0);
    }
    else if (rightButtonStatus > 0) {
        // right mouse drag
        printf("emitting RIGHT_MOUSE_DRAGGED\n");
        emit_mouse_evt(RIGHT_MOUSE_DRAGGED, current.x, current.y, 0);
    }
    else {
        // mouse move
        printf("emitting MOUSE_MOVED\n");
        emit_mouse_evt(MOUSE_MOVED, current.x, current.y, 0);
    }
    return 0;
}

void cursor_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int len,
                    mpr_type type, const void *val, mpr_time time)
{
    if (!val)
        return;
    float *valf = (float*)val;
    current.x = valf[0];
    current.y = valf[1];
    updated = 1;
}

void drag_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int len,
                  mpr_type type, const void *val, mpr_time time)
{
    switch (evt) {
        case MPR_SIG_INST_NEW:
//            if (sign(leftButtonStatus) != -1)
//                break;
            leftButtonStatus = 2;
            updated = 1;
            break;
        case MPR_SIG_REL_UPSTRM:
//            if (sign(leftButtonStatus) != 1)
//                break;
            mpr_sig_release_inst(sig, inst);
            leftButtonStatus = -2;
            updated = 1;
            break;
        case MPR_SIG_UPDATE: {
            if (!val)
                return;
            float *valf = (float*)val;
            current.x = valf[0];
            current.y = valf[1];
            updated = 1;
            break;
        }
        default:
            break;
    }
}

void left_button_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int len,
                         mpr_type type, const void *val, mpr_time time)
{
    int status = val ? sign(*(int*)val) : -1;
    if (status == sign(leftButtonStatus)) // no change
        return;
    leftButtonStatus += status * 2;
    updated = 1;
}

void right_button_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int len,
                          mpr_type type, const void *val, mpr_time time)
{
    int status = val ? sign(*(int*)val) : -1;
    if (status == sign(rightButtonStatus)) // no change
        return;
    rightButtonStatus += status * 2;
    updated = 1;
}

void scroll_handler(mpr_sig sig, mpr_sig_evt evt, mpr_id inst, int len,
                    mpr_type type, const void *val, mpr_time time)
{
    if (!val)
        return;
    float *position = (float*)val;
    emit_mouse_evt(SCROLL_WHEEL, position[0], position[1], 1);
}

void zoom_handler(mpr_sig sig, mpr_id inst, int len, mpr_type type,
                  const void *val, mpr_time time)
{
    ;
}

void rotation_handler(mpr_sig sig, mpr_id inst, int len, mpr_type type,
                      const void *val, mpr_time time)
{
    ;
}

mpr_dev start_mpr_dev(const char *name) {
    printf("starting mpr_dev with name %s\n", name);
    mpr_dev d = mpr_dev_new(name, 0);

    int one = 1;
    float minf[2] = {0.f, 0.f}, maxf[2] = {1.f, 1.f};
    mpr_sig_new(d, MPR_DIR_IN, "position", 2, 'f', "normalized", minf, maxf,
                NULL, cursor_handler, MPR_SIG_UPDATE);
    mpr_sig_new(d, MPR_DIR_IN, "drag", 2, 'f', "normalized", minf, maxf,
                &one, drag_handler, MPR_SIG_ALL);

    minf[0] = minf[1] = -1.f;
    mpr_sig_new(d, MPR_DIR_IN, "scrollWheel", 2, 'f', "normalized", minf, maxf,
                NULL, scroll_handler, MPR_SIG_UPDATE);

    int mini = 0, maxi = 1;
    mpr_sig_new(d, MPR_DIR_IN, "button/left", 1, 'i', NULL, &mini, &maxi,
                NULL, left_button_handler, MPR_SIG_UPDATE);
    mpr_sig_new(d, MPR_DIR_IN, "button/right", 1, 'i', NULL, &mini, &maxi,
                NULL, right_button_handler, MPR_SIG_UPDATE);

//    mpr_sig_new(d, MPR_DIR_IN, "zoom", 1, 'f', "normalized", min, max,
//                &one, zoom_handler, MPR_SIG_UPDATE);
//    min[0] = -M_PI;
//    max[0] = M_PI;
//    mpr_sig_new(d, MPR_DIR_IN, "rotation", 1, 'f', "radians", min, max,
//                &one, rotation_handler, MPR_SIG_UPDATE);
    return d;
}

int poll_mpr_dev(mpr_dev d) {
    updated = 0;
    mpr_dev_poll(d, 1);
    if (updated)
        emit();
    return 0;
}

void quit_mpr_dev(mpr_dev d) {
    printf("freeing mpr dev %s\n", mpr_obj_get_prop_as_str(d, MPR_PROP_NAME, NULL));
    mpr_dev_free(d);
}
