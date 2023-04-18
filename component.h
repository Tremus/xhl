#ifndef XHL_COMPONENT_H
#define XHL_COMPONENT_H

/**
 * Quick and dirty component heirachy.
 * Should be easily extendable
 * No allocations or includes
*/

enum xcomp_event
{
    XCOMP_EVENT_POSITION_CHANGED,
    XCOMP_EVENT_SIZE_CHANGED,
    XCOMP_EVENT_BOUNDS_CHANGED,
    // mouse
    XCOMP_EVENT_MOUSE_ENTER,
    XCOMP_EVENT_MOUSE_EXIT,
    XCOMP_EVENT_MOUSE_MOVE,
    XCOMP_EVENT_MOUSE_UP,
    XCOMP_EVENT_MOUSE_DOWN,
    XCOMP_EVENT_MOUSE_DOUBLE_CLICK,
    XCOMP_EVENT_MOUSE_DRAG,
    XCOMP_EVENT_MOUSE_DROP,
    XCOMP_EVENT_MOUSE_WHEEL,
    XCOMP_EVENT_MOUSE_PINCH,
    XCOMP_EVENT_KEY_DOWN,
    XCOMP_EVENT_KEY_UP,
    XCOMP_EVENT_VISIBILITY_CHANGED,
    XCOMP_EVENT_KEYBOARD_FOCUS_CHANGED,
    XCOMP_EVENT_PAINT,
};

enum xcomp_flag
{
    XCOMP_FLAG_IS_HIDDEN = 1 << 0,
    XCOMP_FLAG_IS_MOUSE_OVER = 1 << 1,
    XCOMP_FLAG_IS_DRAGGING = 1 << 2,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1 << 3,
};

struct xcomp_position { float x; float y; };

union xcomp_event_data
{
    struct xcomp_position position;
    unsigned long raw;
};

struct xcomp_component
{
    xcomp_component* parent;
    xcomp_component* children;
    int num_children;
    int cap_children;

    // coordinates
    float x;
    float y;
    float width;
    float height;

    long flags;
    int (*handle_event)(xcomp_component*, int event, xcomp_event_data data);
};

struct xcomp_root
{
    xcomp_component* root_component;
    xcomp_component* current_mouse_over;
    xcomp_component* current_keyboard_focus;
};

// COMPONENT METHODS

// set x/y/w/h on component, then send event
void xcomp_set_bounds(xcomp_component* comp, float x, float y, float width, float height);

void xcomp_set_position(xcomp_component* comp, float x, float y);
void xcomp_set_size(xcomp_component* comp, float width, float height);

// resizes num children
void xcomp_add_child(xcomp_component* comp, xcomp_component* child);
// resizes num children
void xcomp_remove_child(xcomp_component* comp, xcomp_component* child);
// set flag, send event
void xcomp_set_visible(xcomp_component* comp, int visible);
// checks through parent heirarchy until it finds the root 
void xcomp_get_root_component(xcomp_component* comp);
// returns 0/1 dependon on weather the coordinate lies within component bounds
int xcomp_hit_test(xcomp_component*, float x, float y);

// recursively loops though children until it finds the bottom level component
// with the given coordinates
xcomp_component* xcomp_find_child_at(xcomp_component*, float x, float y);

// recursively looks though parent heirachy until it finds top level component
// with the given coordinates. May return NULL if coordinate is outside of
// screen bounds
xcomp_component* xcomp_find_parent_at(xcomp_component*, float x, float y);

// APP METHODS
void xcomp_send_mouse_message(xcomp_root*, float x, float y);
void xcomp_send_keyboard_message(xcomp_root*, int charcode, int isdown);


#ifdef XHL_COMPONENT_IMPL

// TODO

#endif // XHL_COMPONENT_IMPL
#endif // XHL_COMPONENT_H
