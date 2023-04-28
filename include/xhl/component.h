#pragma once

/**
 * Quick and dirty component heirachy.
 * Should be easily extendable
 * No allocations
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum xcomp_event : uint32_t
{
    XCOMP_EVENT_POSITION_CHANGED,
    XCOMP_EVENT_SIZE_CHANGED,
    XCOMP_EVENT_DIMENSION_CHANGED,
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

enum xcomp_flag : uint64_t
{
    XCOMP_FLAG_IS_HIDDEN = 1ul << 0,
    XCOMP_FLAG_IS_MOUSE_OVER = 1ul << 1,
    XCOMP_FLAG_IS_MOUSE_DOWN = 1ul << 2,
    XCOMP_FLAG_IS_DRAGGING = 1ul << 3,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1ul << 4,
    XCOMP_FLAG_OWNS_CHILDREN = 1ul << 5,
};

union xcomp_position
{
    struct
    {
        float x;
        float y;
    };
    float data[2]; // vec2
};
typedef union xcomp_position xcomp_position;

union xcomp_dimensions
{
    struct
    {
        float x;
        float y;
        float width;
        float height;
    };
    float data[4]; // vec4
};
typedef union xcomp_dimensions xcomp_dimensions;

union xcomp_event_data
{
    uint64_t raw;
    xcomp_position position;
};
typedef union xcomp_event_data xcomp_event_data;

struct xcomp_component
{
    xcomp_dimensions dimensions;

    struct xcomp_component* parent;
    struct xcomp_component** children;
    size_t num_children;
    size_t cap_children;

    uint64_t flags;
    bool (*event_handler) (struct xcomp_component*, uint32_t event,
                           xcomp_event_data data);

    // Keep a ptr to your data here
    void* data;
};
typedef struct xcomp_component xcomp_component;

struct xcomp_root
{
    xcomp_component* main; // root level component
    xcomp_component* mouse_over;
    xcomp_component* keyboard_focus;
};
typedef struct xcomp_root xcomp_root;

// GEOMETRY METHODS

inline bool xcomp_is_emtpy (xcomp_dimensions d)
{
    return d.x == 0.0f && d.y == 0.0f && d.width == 0.0f && d.height == 0.0f;
}

// Check coordinate lies within dimensions
inline bool xcomp_hit_test (xcomp_dimensions d, float mx, float my)
{
    float r = d.x + d.width;
    float b = d.y + d.height;
    return mx >= d.x && my >= d.y && mx <= r && my <= b;
}

// COMPONENT METHODS

void xcomp_init (xcomp_component* comp, void* data);

bool xcomp_empty_event_cb (xcomp_component*, uint32_t, xcomp_event_data);

// set x/y/w/h on component, then send event
void xcomp_set_dimensions (xcomp_component* comp, xcomp_dimensions dimensions);

void xcomp_set_position (xcomp_component* comp, xcomp_position);
void xcomp_set_size (xcomp_component* comp, float width, float height);

// resizes num children
void xcomp_add_child (xcomp_component* comp, xcomp_component* child);
// resizes num children
void xcomp_remove_child (xcomp_component* comp, xcomp_component* child);
// set flag, send event
void xcomp_set_visible (xcomp_component* comp, bool visible);
// checks through parent heirarchy until it finds the root
xcomp_component* xcomp_get_root_component (xcomp_component* comp);

// recursively loops though children until it finds the bottom level
// component with the given coordinates
xcomp_component* xcomp_find_child_at (xcomp_component*, float x, float y);

// recursively looks though parent heirachy until it finds top level
// component with the given coordinates. May return NULL if coordinate is
// outside of screen dimensions
xcomp_component* xcomp_find_parent_at (xcomp_component*, float x, float y);

// APP METHODS
void xcomp_send_mouse_message (xcomp_root*, float x, float y,
                               xcomp_event_data info);
void xcomp_send_keyboard_message (xcomp_root*, uint32_t charcode,
                                  xcomp_event_data info);

#ifdef __cplusplus
}
#endif

// #define XHL_COMPONENT_IMPL
#ifdef XHL_COMPONENT_IMPL

#ifndef XCOMP_INITIAL_CHILDREN_CAP
#define XCOMP_INITIAL_CHILDREN_CAP 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xcomp_init (xcomp_component* comp, void* data)
{
    comp->parent = NULL;
    comp->children = NULL;
    comp->num_children = 0;
    comp->cap_children = 0;

    comp->dimensions.x = 0.0f;
    comp->dimensions.y = 0.0f;
    comp->dimensions.width = 0.0f;
    comp->dimensions.height = 0.0f;

    comp->flags = 0;
    comp->event_handler = &xcomp_empty_event_cb;
    comp->data = data;
}

bool xcomp_empty_event_cb (xcomp_component*, uint32_t, xcomp_event_data)
{
    return true;
}

void xcomp_set_dimensions (xcomp_component* comp, xcomp_dimensions dimensions)
{
    comp->dimensions.x = dimensions.x;
    comp->dimensions.y = dimensions.y;
    comp->dimensions.width = dimensions.width;
    comp->dimensions.height = dimensions.height;

    xcomp_event_data data = {.raw = 0ul};
    comp->event_handler (comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->event_handler (comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->event_handler (comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_position (xcomp_component* comp, xcomp_position pos)
{
    comp->dimensions.x = pos.x;
    comp->dimensions.y = pos.y;

    xcomp_event_data data = {.raw = 0ul};
    comp->event_handler (comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->event_handler (comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_size (xcomp_component* comp, float width, float height)
{
    comp->dimensions.width = width;
    comp->dimensions.height = height;
    xcomp_event_data data = {.raw = 0ul};
    comp->event_handler (comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->event_handler (comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_add_child (xcomp_component* comp, xcomp_component* child)
{
    comp->children[comp->num_children] = child;
    child->parent = comp;
    comp->num_children += 1;
}

void xcomp_remove_child (xcomp_component* comp, xcomp_component* child)
{
    bool child_was_removed = false;
    size_t i = 0;

    // Search through nodes an zero the child comp
    for (; i < comp->num_children; i++)
    {
        // try and remove child
        if (comp->children[i] == child)
        {
            child->parent = nullptr;
            comp->children[i] = nullptr;
            child_was_removed = true;
            break;
        }
    }
    if (child_was_removed)
    {
        // shuffle children back one place
        i += 1;
        for (; i < comp->num_children; i++)
            comp->children[i - 1] = comp->children[i];

        comp->num_children -= 1;
    }
}

void xcomp_set_visible (xcomp_component* comp, bool visible)
{
    if (visible)
        comp->flags &= ~XCOMP_FLAG_IS_HIDDEN;
    else
        comp->flags |= XCOMP_FLAG_IS_HIDDEN;

    xcomp_event_data data = {.raw = visible};
    comp->event_handler (comp, XCOMP_EVENT_VISIBILITY_CHANGED, data);
}

xcomp_component* xcomp_get_root_component (xcomp_component* comp)
{
    xcomp_component* c = comp;

    while (c->parent != NULL)
        c = c->parent;

    return c;
}

xcomp_component* xcomp_find_child_at (xcomp_component* comp, float x, float y)
{
    for (size_t i = comp->num_children; i-- > 0;)
    {
        if (xcomp_hit_test (comp->children[i]->dimensions, x, y))
            return xcomp_find_child_at (comp->children[i], x, y);
    }

    return comp;
}

xcomp_component* xcomp_find_parent_at (xcomp_component* comp, float x, float y)
{
    if (comp->parent != NULL &&
        !xcomp_hit_test (comp->parent->dimensions, x, y))
        return xcomp_find_parent_at (comp->parent, x, y);

    return NULL;
}

void xcomp_send_mouse_enter (xcomp_component* comp, xcomp_event_data info)
{
    comp->flags |= XCOMP_FLAG_IS_MOUSE_OVER;
    comp->event_handler (comp, XCOMP_EVENT_MOUSE_ENTER, info);
    comp->event_handler (comp, XCOMP_EVENT_MOUSE_MOVE, info);
}

void xcomp_send_mouse_exit (xcomp_component* comp, xcomp_event_data info)
{
    comp->flags &= ~XCOMP_FLAG_IS_MOUSE_OVER;
    comp->event_handler (comp, XCOMP_EVENT_MOUSE_EXIT, info);
}

void xcomp_send_mouse_message (xcomp_root* root, float x, float y,
                               xcomp_event_data info)
{

    xcomp_component* last_over = root->mouse_over;
    xcomp_component* next_over = NULL;

    if (last_over == NULL)
    {
        if (xcomp_hit_test (root->main->dimensions, x, y))
        {
            next_over = xcomp_find_child_at (root->main, x, y);

            root->mouse_over = next_over;

            xcomp_send_mouse_enter (next_over, info);
        }
    }
    else
    {

        // check mouse still over
        if (xcomp_hit_test (last_over->dimensions, x, y))
            next_over = xcomp_find_child_at (last_over, x, y);
        else // mouse exited
            next_over = xcomp_find_parent_at (last_over, x, y);

        root->mouse_over = next_over;

        // if failed finding child
        if (last_over == next_over)
        {
            next_over->event_handler (next_over, XCOMP_EVENT_MOUSE_MOVE, info);
        }
        else
        {
            // mouse moved to child or parent
            xcomp_send_mouse_exit (last_over, info);

            if (next_over != NULL)
                xcomp_send_mouse_enter (next_over, info);
        }
    }
    // TODO: handle mouse down
    // TODO: handle mouse wheel
    // TODO: handle mouse pinch
}

void xcomp_send_keyboard_message (xcomp_root*, uint32_t charcode,
                                  xcomp_event_data info)
{
    // TODO
}

#ifdef __cplusplus
}
#endif

#endif // XHL_COMPONENT_IMPL
