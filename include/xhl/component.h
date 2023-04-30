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
    XCOMP_EVENT_PAINT,
    // geometry
    XCOMP_EVENT_POSITION_CHANGED,
    XCOMP_EVENT_SIZE_CHANGED,
    XCOMP_EVENT_DIMENSION_CHANGED,
    XCOMP_EVENT_VISIBILITY_CHANGED,
    // mouse
    XCOMP_EVENT_MOUSE_ENTER,
    XCOMP_EVENT_MOUSE_EXIT,
    XCOMP_EVENT_MOUSE_MOVE,
    XCOMP_EVENT_MOUSE_LEFT_DOWN,
    XCOMP_EVENT_MOUSE_LEFT_UP,
    XCOMP_EVENT_MOUSE_LEFT_CLICK,
    XCOMP_EVENT_MOUSE_LEFT_DOUBLE_CLICK,
    XCOMP_EVENT_MOUSE_RIGHT_DOWN,
    XCOMP_EVENT_MOUSE_RIGHT_UP,
    XCOMP_EVENT_MOUSE_RIGHT_CLICK,
    XCOMP_EVENT_MOUSE_MIDDLE_DOWN,
    XCOMP_EVENT_MOUSE_MIDDLE_UP,
    XCOMP_EVENT_MOUSE_MIDDLE_CLICK,
    XCOMP_EVENT_MOUSE_DRAG,
    XCOMP_EVENT_MOUSE_DROP,
    XCOMP_EVENT_MOUSE_WHEEL,
    XCOMP_EVENT_MOUSE_PINCH,
    // keyboard
    XCOMP_EVENT_KEY_DOWN,
    XCOMP_EVENT_KEY_UP,
    XCOMP_EVENT_KEYBOARD_FOCUS_CHANGED,
};

enum xcomp_flag : uint64_t
{
    XCOMP_FLAG_IS_HIDDEN = 1ul << 0,
    XCOMP_FLAG_IS_MOUSE_OVER = 1ul << 1,
    XCOMP_FLAG_IS_MOUSE_LEFT_DOWN = 1ul << 2,
    XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN = 1ul << 3,
    XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN = 1ul << 4,
    XCOMP_FLAG_IS_DRAGGING = 1ul << 5,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1ul << 6,
    XCOMP_FLAG_OWNS_CHILDREN = 1ul << 7,
};

enum xcomp_modifier : uint64_t
{
    XCOMP_MOD_LEFT_BUTTON = 1 << 0,
    XCOMP_MOD_RIGHT_BUTTON = 1 << 1,
    XCOMP_MOD_MIDDLE_BUTTON = 1 << 2,
    XCOMP_MOD_CTRL_BUTTON = 1 << 3,
    XCOMP_MOD_ALT_BUTTON = 1 << 4,
    XCOMP_MOD_SHIFT_BUTTON = 1 << 5,
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
    struct
    {
        float x;
        float y;
        uint64_t modifiers;
    };
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
    xcomp_component* mouse_left_down;
    xcomp_component* mouse_right_down;
    xcomp_component* mouse_middle_down;
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
void xcomp_send_mouse_position (xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_down (xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_up (xcomp_root*, xcomp_event_data info);
void xcomp_send_keyboard_message (xcomp_root*, xcomp_event_data info);

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
            child->parent = NULL;
            comp->children[i] = NULL;
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
        ! xcomp_hit_test (comp->parent->dimensions, x, y))
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
    if (comp->flags & XCOMP_EVENT_MOUSE_LEFT_DOWN)
    {
        comp->flags &= ~XCOMP_EVENT_MOUSE_LEFT_DOWN;
        comp->event_handler (comp, XCOMP_EVENT_MOUSE_LEFT_UP, info);
    }
    if (comp->flags & XCOMP_EVENT_MOUSE_RIGHT_DOWN)
    {
        comp->flags &= ~XCOMP_EVENT_MOUSE_RIGHT_DOWN;
        comp->event_handler (comp, XCOMP_EVENT_MOUSE_RIGHT_UP, info);
    }
    if (comp->flags & XCOMP_EVENT_MOUSE_MIDDLE_DOWN)
    {
        comp->flags &= ~XCOMP_EVENT_MOUSE_MIDDLE_DOWN;
        comp->event_handler (comp, XCOMP_EVENT_MOUSE_MIDDLE_UP, info);
    }

    comp->flags &= ~XCOMP_FLAG_IS_MOUSE_OVER;
    comp->event_handler (comp, XCOMP_EVENT_MOUSE_EXIT, info);
}

void xcomp_send_mouse_position (xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* last_over = root->mouse_over;
    xcomp_component* next_over = NULL;

    if (last_over == NULL)
    {
        if (xcomp_hit_test (root->main->dimensions, info.x, info.y))
        {
            next_over = xcomp_find_child_at (root->main, info.x, info.y);

            root->mouse_over = next_over;

            xcomp_send_mouse_enter (next_over, info);
        }
    }
    else
    {
        // check mouse still over
        if (xcomp_hit_test (last_over->dimensions, info.x, info.y))
            next_over = xcomp_find_child_at (last_over, info.x, info.y);
        else // mouse exited
            next_over = xcomp_find_parent_at (last_over, info.x, info.y);

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

void xcomp_send_mouse_down (xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* comp = xcomp_find_child_at (root->main, info.x, info.y);
    if (comp != NULL)
    {
        // handle left button
        if ((info.modifiers & XCOMP_MOD_LEFT_BUTTON) &&
            root->mouse_left_down == NULL)
        {
            root->mouse_left_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
            comp->event_handler (comp, XCOMP_EVENT_MOUSE_LEFT_DOWN, info);
        }

        // handle right button
        if ((info.modifiers & XCOMP_MOD_RIGHT_BUTTON) &&
            root->mouse_right_down == NULL)
        {
            root->mouse_right_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
            comp->event_handler (comp, XCOMP_EVENT_MOUSE_RIGHT_DOWN, info);
        }

        // handle middle button
        if ((info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) &&
            root->mouse_middle_down == NULL)
        {
            root->mouse_middle_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
            comp->event_handler (comp, XCOMP_EVENT_MOUSE_MIDDLE_DOWN, info);
        }
    }
}

void xcomp_send_mouse_up (xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* comp = xcomp_find_child_at (root->main, info.x, info.y);

    // handle left button
    if ((info.modifiers & XCOMP_MOD_LEFT_BUTTON) &&
        root->mouse_left_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_left_down;

        root->mouse_left_down = NULL;
        last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
        last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_LEFT_UP, info);

        if (last_comp == comp)
        {
            last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_LEFT_CLICK,
                                      info);
            // TODO: record time and component in case of a double click
        }
    }

    // handle right button
    if ((info.modifiers & XCOMP_MOD_RIGHT_BUTTON) &&
        root->mouse_right_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_right_down;

        root->mouse_right_down = NULL;
        last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
        last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_RIGHT_UP, info);

        if (last_comp == comp)
        {
            // We probably don't care about right double clicks
            last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_RIGHT_CLICK,
                                      info);
        }
    }

    // handle middle button
    if ((info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) &&
        root->mouse_middle_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_middle_down;

        root->mouse_middle_down = NULL;
        last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
        last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_MIDDLE_UP, info);

        if (last_comp == comp)
        {
            // We probably don't care about middle double clicks
            last_comp->event_handler (last_comp, XCOMP_EVENT_MOUSE_MIDDLE_CLICK,
                                      info);
        }
    }
}

void xcomp_send_keyboard_message (xcomp_root*, xcomp_event_data info)
{
    // TODO
}

#ifdef __cplusplus
}
#endif

#endif // XHL_COMPONENT_IMPL
