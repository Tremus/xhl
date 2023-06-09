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
    XCOMP_EVENT_ENABLEMENT_CHANGED,
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
    XCOMP_EVENT_MOUSE_WHEEL,
    XCOMP_EVENT_MOUSE_PINCH,
    // Dragging
    // These are sent to the component being dragged
    XCOMP_EVENT_DRAG_START,
    XCOMP_EVENT_DRAG_MOVE,
    XCOMP_EVENT_DRAG_END,
    // These are sent to the component being dragged over
    XCOMP_EVENT_DRAG_ENTER,
    XCOMP_EVENT_DRAG_EXIT,
    XCOMP_EVENT_DRAG_DROP,
    // keyboard
    XCOMP_EVENT_KEY_DOWN,
    XCOMP_EVENT_KEY_UP,
    XCOMP_EVENT_KEYBOARD_FOCUS_CHANGED,
};

enum xcomp_flag : uint64_t
{
    XCOMP_FLAG_IS_HIDDEN            = 1 << 0,
    XCOMP_FLAG_IS_MOUSE_OVER        = 1 << 1,
    XCOMP_FLAG_IS_MOUSE_LEFT_DOWN   = 1 << 2,
    XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN = 1 << 3,
    XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN  = 1 << 4,
    XCOMP_FLAG_IS_DRAGGING          = 1 << 5,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1 << 6,
    XCOMP_FLAG_HAS_KEYBOARD_FOCUS   = 1 << 7,
    XCOMP_FLAG_OWNS_CHILDREN        = 1 << 8,
    XCOMP_FLAG_IS_DISABLED          = 1 << 9,
    XCOMP_FLAG_IS_DROP_TARGET       = 1 << 10,
};

enum xcomp_modifier : uint64_t
{
    XCOMP_MOD_LEFT_BUTTON   = 1 << 0,
    XCOMP_MOD_RIGHT_BUTTON  = 1 << 1,
    XCOMP_MOD_MIDDLE_BUTTON = 1 << 2,
    XCOMP_MOD_CTRL_BUTTON   = 1 << 3,
    XCOMP_MOD_ALT_BUTTON    = 1 << 4,
    XCOMP_MOD_SHIFT_BUTTON  = 1 << 5,
    // Flag set when using 2+ finger gestures on Apple devices
    // See: [NSEvent hasPreciseScrollingDeltas]
    XCOMP_MOD_PRECISE_SCROLL = 1 << 6,
    // Flag set when touch events are inverted on Apple devices
    // See: [NSEvent isDirectionInvertedFromDevice]
    XCOMP_MOD_INVERTED_SCROLL = 1 << 7,
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

union xcomp_size
{
    struct
    {
        float width;
        float height;
    };
    float data[2]; // vec2
};
typedef union xcomp_size xcomp_size;

union xcomp_dimensions
{
    struct
    {
        float x;
        float y;
        float width;
        float height;
    };
    struct
    {
        xcomp_position position;
        xcomp_size size;
    };
    float data[4]; // vec4
};
typedef union xcomp_dimensions xcomp_dimensions;

union xcomp_event_data
{
    uint64_t raw;
    void* ptr;
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
    // size_t cap_children;

    uint64_t flags;
    void (*event_handler)(
        struct xcomp_component*,
        uint32_t event,
        xcomp_event_data data);

    // Keep a ptr to your data here
    void* data;
};
typedef struct xcomp_component xcomp_component;

struct xcomp_root
{
    // None of the following these pointers are owned
    // Main is your root level component
    xcomp_component* main;
    // These hold state of basic mouse & keyboard events
    // To keep this state clean, call xcomp_root_clear()
    // BEFORE you DELETE any component or AFTER you change
    // the VISIBILITY of a component
    // See @xcomp_root_clear()
    xcomp_component* mouse_over;
    xcomp_component* mouse_left_down;
    xcomp_component* mouse_right_down;
    xcomp_component* mouse_middle_down;
    xcomp_component* keyboard_focus;

    xcomp_position position;
};
typedef struct xcomp_root xcomp_root;

// GEOMETRY METHODS

// Check is not all 0s
inline bool xcomp_is_empty(xcomp_dimensions d);
// Check mod flags for a popup menu
inline bool xcomp_is_popup_menu(uint64_t mods);
// Check coordinate lies within dimensions
inline bool xcomp_hit_test(xcomp_dimensions d, xcomp_position pos);
inline xcomp_position xcomp_centre(xcomp_dimensions d);

// COMPONENT METHODS

void xcomp_init(xcomp_component* comp, void* data);

void xcomp_empty_event_cb(xcomp_component*, uint32_t, xcomp_event_data);

// set x/y/w/h on component, then send event
void xcomp_set_dimensions(xcomp_component* comp, xcomp_dimensions dimensions);

void xcomp_set_position(xcomp_component* comp, xcomp_position);
void xcomp_set_size(xcomp_component* comp, float width, float height);

// resizes num children
void xcomp_add_child(xcomp_component* comp, xcomp_component* child);
// resizes num children
void xcomp_remove_child(xcomp_component* comp, xcomp_component* child);
// set flag, send event
// You should call xcomp_root_clear() after using this!
void xcomp_set_visible(xcomp_component* comp, bool visible);
// You should call xcomp_root_clear() after using this!
void xcomp_set_enabled(xcomp_component* comp, bool enabled);
inline bool xcomp_is_hidden(xcomp_component*);
inline bool xcomp_is_enabled(xcomp_component*);
// checks through parent heirarchy until it finds the root
xcomp_component* xcomp_get_root_component(xcomp_component* comp);

// recursively loops though children until it finds the bottom level
// component with the given coordinates
xcomp_component* xcomp_find_child_at(xcomp_component*, xcomp_position);

// recursively looks though parent heirachy until it finds top level
// component with the given coordinates. May return NULL if coordinate is
// outside of screen dimensions
xcomp_component* xcomp_find_parent_at(xcomp_component*, xcomp_position);

// ROOT METHODS
void xcomp_send_mouse_position(xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_down(xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_up(xcomp_root*, xcomp_event_data info);
void xcomp_send_keyboard_message(xcomp_root*, xcomp_event_data info);
// set component to NULL to remove focus
void xcomp_root_give_keyboard_focus(xcomp_root*, xcomp_component* comp);
// This will flush and update the properties of xcomp_root
// Call this before any component gets deleted to 0 any dangling pointers
// Call this after the visibility of any component changes
// Call this after enabling / disabling any components
void xcomp_root_clear(xcomp_root*);

// INLINE IMPLEMENTATIONS

bool xcomp_is_empty(xcomp_dimensions d)
{
    return d.width == 0.0f || d.height == 0.0f;
}

bool xcomp_is_popup_menu(uint64_t mods)
{
    return
#ifdef __APPLE__
        ((mods & (XCOMP_MOD_CTRL_BUTTON | XCOMP_MOD_LEFT_BUTTON)) ==
         (XCOMP_MOD_CTRL_BUTTON | XCOMP_MOD_LEFT_BUTTON)) ||
#endif
        (mods & XCOMP_MOD_RIGHT_BUTTON);
}

bool xcomp_hit_test(xcomp_dimensions d, xcomp_position pos)
{
    float r = d.x + d.width;
    float b = d.y + d.height;
    return pos.x >= d.x && pos.y >= d.y && pos.x <= r && pos.y <= b;
}

xcomp_position xcomp_centre(xcomp_dimensions d)
{
    return {.data = {0.5f * d.width + d.x, 0.5f * d.height + d.y}};
}

bool xcomp_is_hidden(xcomp_component* comp)
{
    return (comp->flags & XCOMP_FLAG_IS_HIDDEN) == XCOMP_FLAG_IS_HIDDEN;
}

bool xcomp_is_enabled(xcomp_component* comp)
{
    return (comp->flags & XCOMP_FLAG_IS_DISABLED) != XCOMP_FLAG_IS_DISABLED;
}

#ifdef __cplusplus
}
#endif

// #define XHL_COMPONENT_IMPL
#ifdef XHL_COMPONENT_IMPL

#ifdef __cplusplus
extern "C" {
#endif

void xcomp_send_mouse_exit(xcomp_component* comp, xcomp_event_data info);

void xcomp_root_give_keyboard_focus(
    xcomp_root* root,
    xcomp_component* next_comp)
{
    xcomp_component* last_comp = root->keyboard_focus;
    xcomp_event_data edata;
    edata.x         = root->position.x;
    edata.y         = root->position.y;
    edata.modifiers = 0;

    root->keyboard_focus = next_comp;
    if (last_comp != NULL &&
        (last_comp->flags & XCOMP_FLAG_HAS_KEYBOARD_FOCUS) ==
            XCOMP_FLAG_HAS_KEYBOARD_FOCUS)
    {
        last_comp->flags &= ~XCOMP_FLAG_HAS_KEYBOARD_FOCUS;
        last_comp->event_handler(
            last_comp,
            XCOMP_EVENT_KEYBOARD_FOCUS_CHANGED,
            edata);
    }

    // check should take keyboard focus
    if (next_comp != NULL &&
        (next_comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS) ==
            XCOMP_FLAG_WANTS_KEYBOARD_FOCUS &&
        (next_comp->flags & XCOMP_FLAG_HAS_KEYBOARD_FOCUS) !=
            XCOMP_FLAG_HAS_KEYBOARD_FOCUS)
    {
        next_comp->flags |= XCOMP_FLAG_HAS_KEYBOARD_FOCUS;
        next_comp->event_handler(
            next_comp,
            XCOMP_EVENT_KEYBOARD_FOCUS_CHANGED,
            edata);
    }
}

void xcomp_init(xcomp_component* comp, void* data)
{
    memset(comp, 0, sizeof(*comp));

    comp->event_handler = &xcomp_empty_event_cb;
    comp->data          = data;
}

void xcomp_empty_event_cb(xcomp_component*, uint32_t, xcomp_event_data) {}

void xcomp_set_dimensions(xcomp_component* comp, xcomp_dimensions dimensions)
{
    comp->dimensions.x      = dimensions.x;
    comp->dimensions.y      = dimensions.y;
    comp->dimensions.width  = dimensions.width;
    comp->dimensions.height = dimensions.height;

    xcomp_event_data data = {.raw = 0ul};
    comp->event_handler(comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->event_handler(comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->event_handler(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_position(xcomp_component* comp, xcomp_position pos)
{
    comp->dimensions.x = pos.x;
    comp->dimensions.y = pos.y;

    xcomp_event_data data = {.raw = 0ul};
    comp->event_handler(comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->event_handler(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_size(xcomp_component* comp, float width, float height)
{
    comp->dimensions.width  = width;
    comp->dimensions.height = height;
    xcomp_event_data data   = {.raw = 0ul};
    comp->event_handler(comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->event_handler(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_add_child(xcomp_component* comp, xcomp_component* child)
{
    comp->children[comp->num_children] = child;
    child->parent                      = comp;
    comp->num_children += 1;
}

void xcomp_remove_child(xcomp_component* comp, xcomp_component* child)
{
    bool child_was_removed = false;
    size_t i               = 0;

    // Search through nodes an zero the child comp
    for (; i < comp->num_children; i++)
    {
        // try and remove child
        if (comp->children[i] == child)
        {
            child->parent     = NULL;
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

void xcomp_set_visible(xcomp_component* comp, bool visible)
{
    uint64_t prev_flags = comp->flags;
    if (visible)
        comp->flags &= ~XCOMP_FLAG_IS_HIDDEN;
    else
        comp->flags |= XCOMP_FLAG_IS_HIDDEN;

    if (prev_flags != comp->flags)
    {
        xcomp_event_data data = {.raw = visible};
        comp->event_handler(comp, XCOMP_EVENT_VISIBILITY_CHANGED, data);
    }
}

void xcomp_set_enabled(xcomp_component* comp, bool enabled)
{
    uint64_t prev_flags = comp->flags;
    if (enabled)
        comp->flags &= ~XCOMP_FLAG_IS_DISABLED;
    else
        comp->flags |= XCOMP_FLAG_IS_DISABLED;

    if (prev_flags != comp->flags)
    {
        xcomp_event_data data = {.raw = enabled};
        comp->event_handler(comp, XCOMP_EVENT_ENABLEMENT_CHANGED, data);
    }
}

xcomp_component* xcomp_get_root_component(xcomp_component* comp)
{
    xcomp_component* c = comp;

    while (c->parent != NULL)
        c = c->parent;

    return c;
}

xcomp_component* xcomp_find_child_at(xcomp_component* comp, xcomp_position p)
{
    for (size_t i = comp->num_children; i-- > 0;)
    {
        xcomp_component* child = comp->children[i];

        // if visible and mouse within dimensions
        if (! ((child->flags &
                (XCOMP_FLAG_IS_HIDDEN | XCOMP_FLAG_IS_DISABLED)) != 0) &&
            xcomp_hit_test(child->dimensions, p))
            return xcomp_find_child_at(child, p);
    }

    return comp;
}

xcomp_component* xcomp_find_parent_at(xcomp_component* comp, xcomp_position p)
{
    if (comp->parent != NULL && ! xcomp_hit_test(comp->parent->dimensions, p))
        return xcomp_find_parent_at(comp->parent, p);

    return NULL;
}

void xcomp_send_mouse_enter(xcomp_component* comp, xcomp_event_data info)
{
    comp->flags |= XCOMP_FLAG_IS_MOUSE_OVER;
    comp->event_handler(comp, XCOMP_EVENT_MOUSE_ENTER, info);
    comp->event_handler(comp, XCOMP_EVENT_MOUSE_MOVE, info);
}

void xcomp_send_mouse_exit(xcomp_component* comp, xcomp_event_data info)
{
    comp->flags &= ~XCOMP_FLAG_IS_MOUSE_OVER;

    if (comp->flags & XCOMP_FLAG_IS_MOUSE_LEFT_DOWN)
    {
        comp->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
        comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_UP, info);
    }
    if (comp->flags & XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN)
    {
        comp->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
        comp->event_handler(comp, XCOMP_EVENT_MOUSE_RIGHT_UP, info);
    }
    if (comp->flags & XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN)
    {
        comp->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
        comp->event_handler(comp, XCOMP_EVENT_MOUSE_MIDDLE_UP, info);
    }

    comp->event_handler(comp, XCOMP_EVENT_MOUSE_EXIT, info);
}

void xcomp_send_mouse_position(xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* last_over = root->mouse_over;
    xcomp_component* next_over = NULL;

    root->position.x = info.x;
    root->position.y = info.y;

    if (last_over == NULL)
    {
        if (xcomp_hit_test(root->main->dimensions, {info.x, info.y}))
        {
            next_over = xcomp_find_child_at(root->main, {info.x, info.y});

            root->mouse_over = next_over;

            xcomp_send_mouse_enter(next_over, info);
        }
    }
    // Check for drag
    else if (last_over == root->mouse_left_down)
    {
        // Check still dragging
        if (root->mouse_left_down->flags & XCOMP_FLAG_IS_DRAGGING)
        {
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_MOVE, info);
            // TODO: find drop target
        }
        else
        {
            last_over->flags |= XCOMP_FLAG_IS_DRAGGING;
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_START, info);
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_MOVE, info);
        }
    }
    else
    {
        // check mouse still over
        if (xcomp_hit_test(last_over->dimensions, {info.x, info.y}))
            next_over = xcomp_find_child_at(last_over, {info.x, info.y});
        else // mouse exited
            next_over = xcomp_find_parent_at(last_over, {info.x, info.y});

        root->mouse_over = next_over;

        // if failed finding child
        if (last_over == next_over)
        {
            next_over->event_handler(next_over, XCOMP_EVENT_MOUSE_MOVE, info);
        }
        else
        {
            // mouse moved to child or parent
            xcomp_send_mouse_exit(last_over, info);

            if (next_over != NULL)
                xcomp_send_mouse_enter(next_over, info);
        }
    }
    // TODO: handle mouse down
    // TODO: handle mouse wheel
    // TODO: handle mouse pinch
}

void xcomp_send_mouse_down(xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* comp = xcomp_find_child_at(root->main, {info.x, info.y});
    if (comp != NULL)
    {
        // handle left button
        if ((info.modifiers & XCOMP_MOD_LEFT_BUTTON) &&
            root->mouse_left_down == NULL)
        {
            root->mouse_left_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;

            if (comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
                xcomp_root_give_keyboard_focus(root, comp);

            comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_DOWN, info);
        }

        // handle right button
        if ((info.modifiers & XCOMP_MOD_RIGHT_BUTTON) &&
            root->mouse_right_down == NULL)
        {
            root->mouse_right_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;

            if (comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
                xcomp_root_give_keyboard_focus(root, comp);

            comp->event_handler(comp, XCOMP_EVENT_MOUSE_RIGHT_DOWN, info);
        }

        // handle middle button
        if ((info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) &&
            root->mouse_middle_down == NULL)
        {
            root->mouse_middle_down = comp;
            comp->flags |= XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;

            if (comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
                xcomp_root_give_keyboard_focus(root, comp);

            comp->event_handler(comp, XCOMP_EVENT_MOUSE_MIDDLE_DOWN, info);
        }
    }
}

void xcomp_send_mouse_up(xcomp_root* root, xcomp_event_data info)
{
    xcomp_component* comp = xcomp_find_child_at(root->main, {info.x, info.y});

    // handle left button
    if ((info.modifiers & XCOMP_MOD_LEFT_BUTTON) &&
        root->mouse_left_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_left_down;
        root->mouse_left_down      = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_DRAGGING)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_DRAGGING;
            last_comp->event_handler(last_comp, XCOMP_EVENT_DRAG_END, info);
            // TODO: drop on target
        }

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_LEFT_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_LEFT_UP,
                info);
        }

        if (last_comp == comp)
        {
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_LEFT_CLICK,
                info);
            // TODO: record time and component in case of a double click
        }
        // If components do not match, the component must have been dragged into
        // different boundaries. We need to find which component the mouse is
        // hovering over
        else
        {
            xcomp_send_mouse_position(root, info);
        }
    }

    // handle right button
    if ((info.modifiers & XCOMP_MOD_RIGHT_BUTTON) &&
        root->mouse_right_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_right_down;
        root->mouse_right_down     = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_RIGHT_UP,
                info);
        }

        if (last_comp == comp)
        {
            // We probably don't care about right double clicks
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_RIGHT_CLICK,
                info);
        }
    }

    // handle middle button
    if ((info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) &&
        root->mouse_middle_down != NULL)
    {
        xcomp_component* last_comp = root->mouse_middle_down;
        root->mouse_middle_down    = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_MIDDLE_UP,
                info);
        }

        if (last_comp == comp)
        {
            // We probably don't care about middle double clicks
            last_comp->event_handler(
                last_comp,
                XCOMP_EVENT_MOUSE_MIDDLE_CLICK,
                info);
        }
    }
}

void xcomp_send_keyboard_message(xcomp_root*, xcomp_event_data info)
{
    // TODO
}

void xcomp_root_clear(xcomp_root* root)
{
    xcomp_component* last_mouse_over        = root->mouse_over;
    xcomp_component* last_mouse_left_down   = root->mouse_left_down;
    xcomp_component* last_mouse_right_down  = root->mouse_right_down;
    xcomp_component* last_mouse_middle_down = root->mouse_middle_down;
    xcomp_component* last_keyboard_focus    = root->keyboard_focus;
    xcomp_event_data edata                  = {
                         .x         = root->position.x,
                         .y         = root->position.y,
                         .modifiers = 0};

    root->mouse_over        = NULL;
    root->mouse_left_down   = NULL;
    root->mouse_right_down  = NULL;
    root->mouse_middle_down = NULL;

    xcomp_root_give_keyboard_focus(root, NULL);

    if (last_mouse_middle_down != NULL)
    {
        last_mouse_middle_down->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
        last_mouse_middle_down->event_handler(
            last_mouse_middle_down,
            XCOMP_EVENT_MOUSE_MIDDLE_UP,
            edata);
    }

    if (last_mouse_right_down != NULL)
    {
        last_mouse_right_down->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
        last_mouse_right_down->event_handler(
            last_mouse_right_down,
            XCOMP_EVENT_MOUSE_RIGHT_UP,
            edata);
    }

    if (last_mouse_left_down != NULL)
    {
        if (last_mouse_left_down->flags & XCOMP_FLAG_IS_DRAGGING)
        {
            last_mouse_left_down->flags &= ~XCOMP_FLAG_IS_DRAGGING;
            last_mouse_left_down->event_handler(
                last_mouse_left_down,
                XCOMP_EVENT_DRAG_END,
                edata);
        }

        last_mouse_left_down->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
        last_mouse_left_down->event_handler(
            last_mouse_left_down,
            XCOMP_EVENT_MOUSE_LEFT_UP,
            edata);
    }

    if (last_mouse_over != NULL)
        xcomp_send_mouse_exit(last_mouse_over, edata);

    // find new component for mouse to be over
    xcomp_send_mouse_position(root, edata);
}

#ifdef __cplusplus
}
#endif

#endif // XHL_COMPONENT_IMPL
