#ifndef XHL_COMPONENT_H
#define XHL_COMPONENT_H

/**
 * Quick and dirty component heirachy.
 * Should be easily extendable
 * No allocations
 */

#ifdef NDEBUG
#define XCOMP_ASSERT(...)
#else // !NDEBUG

#ifdef _WIN32
#define XCOMP_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else
#define XCOMP_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
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
    XCOMP_EVENT_MOUSE_LEFT_TRIPLE_CLICK,
    XCOMP_EVENT_MOUSE_RIGHT_DOWN,
    XCOMP_EVENT_MOUSE_RIGHT_UP,
    XCOMP_EVENT_MOUSE_RIGHT_CLICK,
    XCOMP_EVENT_MOUSE_MIDDLE_DOWN,
    XCOMP_EVENT_MOUSE_MIDDLE_UP,
    XCOMP_EVENT_MOUSE_MIDDLE_CLICK,
    // Values should be in 120ths. This is the default value on Windows using:
    // case WM_SCROLLWHEEL: {
    //     int deltaY = HIWORD(wParam);
    // macOS is trickier, but the Y value can be obtained using:
    // - (void)scrollWheel:(NSEvent*)event {
    // int deltaX = CGEventGetIntegerValueField([event CGEvent], kCGScrollWheelEventDeltaAxis2) * 120 // x is axis 2
    // int deltaY = CGEventGetIntegerValueField([event CGEvent], kCGScrollWheelEventDeltaAxis1) * 120
    XCOMP_EVENT_MOUSE_SCROLL_WHEEL,
    // See: [NSEvent hasPreciseScrollingDeltas] and [NSEvent phase]
    XCOMP_EVENT_MOUSE_TOUCHPAD_BEGIN,
    XCOMP_EVENT_MOUSE_TOUCHPAD_MOVE,
    XCOMP_EVENT_MOUSE_TOUCHPAD_END,
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
    XCOMP_EVENT_KEY_TEXT, // for sending text to widgets
    XCOMP_EVENT_KEY_DOWN, // for anything else responing to key events
    XCOMP_EVENT_KEY_UP,
    XCOMP_EVENT_KEYBOARD_FOCUS_GAINED,
    XCOMP_EVENT_KEYBOARD_FOCUS_LOST,
};

enum
{
    XCOMP_FLAG_IS_DISABLED          = 1 << 0,
    XCOMP_FLAG_IS_HIDDEN            = 1 << 1,
    XCOMP_FLAG_IS_MOUSE_OVER        = 1 << 2,
    XCOMP_FLAG_IS_MOUSE_LEFT_DOWN   = 1 << 3,
    XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN = 1 << 4,
    XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN  = 1 << 5,
    XCOMP_FLAG_IS_DRAGGING          = 1 << 6,
    // If a dragged component has this flag it will trigger drag
    // enter/exit/drop events on other components
    XCOMP_FLAG_CAN_DRAG_AND_DROP    = 1 << 7,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1 << 8,
    XCOMP_FLAG_HAS_KEYBOARD_FOCUS   = 1 << 9,
};

enum
{
    XCOMP_MOD_LEFT_BUTTON   = 1 << 0,
    XCOMP_MOD_RIGHT_BUTTON  = 1 << 1,
    XCOMP_MOD_MIDDLE_BUTTON = 1 << 2,
    XCOMP_MOD_KEY_CTRL      = 1 << 3,
    XCOMP_MOD_KEY_ALT       = 1 << 4,
    XCOMP_MOD_KEY_SHIFT     = 1 << 5,
    XCOMP_MOD_KEY_CMD       = 1 << 6,
    XCOMP_MOD_KEY_OPTION    = 1 << 7,
    // Flag set when touch events are inverted on Apple devices
    // See: [NSEvent isDirectionInvertedFromDevice]
    XCOMP_MOD_INVERTED_SCROLL = 1 << 8,

#ifdef _WIN32
    XCOMP_MOD_PLATFORM_KEY_CTRL = XCOMP_MOD_KEY_CTRL,
    XCOMP_MOD_PLATFORM_KEY_ALT  = XCOMP_MOD_KEY_ALT,
#elif defined(__APPLE__)
    XCOMP_MOD_PLATFORM_KEY_CTRL = XCOMP_MOD_KEY_CMD,
    XCOMP_MOD_PLATFORM_KEY_ALT  = XCOMP_MOD_KEY_OPTION,
#endif
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
        xcomp_size     size;
    };
    float data[4]; // vec4
};
typedef union xcomp_dimensions xcomp_dimensions;

union xcomp_event_data
{
    uint64_t raw;
    void*    ptr;
    struct
    {
        float    x;
        float    y;
        uint64_t modifiers;
    };
};
typedef union xcomp_event_data xcomp_event_data;

struct xcomp_component
{
    xcomp_dimensions dimensions;

    struct xcomp_component*  parent;
    struct xcomp_component** children;
    size_t                   num_children;
    // size_t cap_children;

    uint64_t flags;
    void (*event_handler)(struct xcomp_component*, uint32_t event, xcomp_event_data data);

    // Keep a ptr to your data here
    void* data;
};
typedef struct xcomp_component xcomp_component;

/* NOTE/IDEA: Below is a 24byte alternative, potentially a good solution for a v2 of xcomp
struct xcomp_component2
{
    uint16_t x, y, w, h;

    uint16_t byte_offset_parent; // If zero, root
    uint16_t byte_offset_prev_sibling; // if zero, first child
    uint16_t byte_offset_last_child; // if zero, no children
    uint16_t flags;

    void (*callback)(struct xcomp_component*, uint32_t event, xcomp_event_data data);
};
*/

typedef struct xcomp_root
{
    // None of the following these pointers are owned
    // Main is your root level component
    xcomp_component* main;
    // These hold state of basic mouse & keyboard events
    // To keep this state clean, see instructions for
    // xcomp_root_clear() & xcomp_root_refresh()
    xcomp_component* mouse_over;
    xcomp_component* mouse_left_down;
    xcomp_component* mouse_last_left_click;
    uint32_t         last_left_click_time;
    uint32_t         left_click_counter;
    xcomp_component* mouse_right_down;
    xcomp_component* mouse_middle_down;
    xcomp_component* mouse_drag_over;
    xcomp_component* keyboard_focus;

    xcomp_position position;
    xcomp_position mouse_down_pos; // Used for preventing 1px drag events that should be clicks
} xcomp_root;

// GEOMETRY METHODS

// Check is not all 0s
static inline bool xcomp_is_empty(xcomp_dimensions d);
// Check mouse & keyboard mod flags for popup menu
static inline bool xcomp_is_popup_menu(uint32_t event, uint64_t mods);
// Check coordinate lies within dimensions
static inline bool           xcomp_hit_test(xcomp_dimensions d, xcomp_position pos);
static inline xcomp_position xcomp_centre(xcomp_dimensions d);

// COMPONENT METHODS

void xcomp_init(xcomp_component* comp, void* data);

void xcomp_empty_event_cb(xcomp_component*, uint32_t, xcomp_event_data);

// set x/y/w/h on component, then send event
void xcomp_set_dimensions(xcomp_component* comp, xcomp_dimensions dimensions);

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

// Traverses backwards through parent hierarchy until it finds the root.
// If you keep a pointer to your whole applicaton on your root component, then
// this is a useful way to retrieve it inside event callbacks from any child in
// your hierarchy
const xcomp_component* xcomp_get_root_component(const xcomp_component* comp);

// recursively loops though children until it finds the bottom level
// component with the given coordinates
xcomp_component* xcomp_find_child_at(xcomp_component*, xcomp_position);

// recursively looks though parent heirachy until it finds top level
// component with the given coordinates. May return NULL if coordinate is
// outside of screen dimensions
xcomp_component* xcomp_find_parent_at(xcomp_component*, xcomp_position);

// Send the same event to every child in tree. Uses depth-first search algorithm.
void xcomp_send_event_to_children_recursive(xcomp_component* comp, uint32_t ev_type, xcomp_event_data info);

// ROOT METHODS
void xcomp_send_mouse_position(xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_down(xcomp_root*, xcomp_event_data info);
void xcomp_send_mouse_up(xcomp_root*, xcomp_event_data, uint32_t time_ms, uint32_t double_click_interval_ms);
void xcomp_send_keyboard_message(xcomp_root*, xcomp_event_data info);
// set component to NULL to remove focus
void xcomp_root_give_keyboard_focus(xcomp_root*, xcomp_component* comp);
// Useful function to call anytime someone presses the TAB keyboard key
void xcomp_root_give_next_sibling_keyboard_focus(xcomp_root*, xcomp_component* comp);
// This will flush and update the properties of xcomp_root
// Call this BEFORE any component gets deleted to 0 any dangling pointers
void xcomp_root_clear(xcomp_root*);
// Similar to xcomp_root_clear
// Call this AFTER toggling components HIDDEN or DISABLED flags
void xcomp_root_refresh(xcomp_root*);

// INLINE IMPLEMENTATIONS

bool xcomp_is_empty(xcomp_dimensions d) { return d.width == 0.0f || d.height == 0.0f; }

bool xcomp_is_popup_menu(uint32_t event, uint64_t mods)
{
#ifdef __APPLE__
    uint64_t lctrl = XCOMP_MOD_KEY_CTRL | XCOMP_MOD_LEFT_BUTTON;
    return event == XCOMP_EVENT_MOUSE_RIGHT_DOWN || (event == XCOMP_EVENT_MOUSE_LEFT_DOWN && (mods & lctrl) == lctrl);
#else
    return event == XCOMP_EVENT_MOUSE_RIGHT_CLICK;
#endif
}

bool xcomp_hit_test(xcomp_dimensions d, xcomp_position pos)
{
    float r = d.x + d.width;
    float b = d.y + d.height;
    return pos.x >= d.x && pos.y >= d.y && pos.x < r && pos.y < b;
}

xcomp_position xcomp_centre(xcomp_dimensions d)
{
    xcomp_position p = {.x = 0.5f * d.width + d.x, .y = 0.5f * d.height + d.y};
    return p;
}

#ifdef __cplusplus
}
#endif
#endif // XHL_COMPONENT_H

#ifdef XHL_COMPONENT_IMPL
#undef XHL_COMPONENT_IMPL

#include <math.h> // fabf, hypotf

#ifdef __cplusplus
extern "C" {
#endif

void xcomp_root_give_keyboard_focus(xcomp_root* root, xcomp_component* next_comp)
{
    xcomp_component* last_comp = root->keyboard_focus;
    xcomp_event_data edata;
    edata.x         = root->position.x;
    edata.y         = root->position.y;
    edata.modifiers = 0;

    root->keyboard_focus = next_comp;
    if (last_comp != NULL && (last_comp->flags & XCOMP_FLAG_HAS_KEYBOARD_FOCUS))
    {
        last_comp->flags &= ~XCOMP_FLAG_HAS_KEYBOARD_FOCUS;
        last_comp->event_handler(last_comp, XCOMP_EVENT_KEYBOARD_FOCUS_LOST, edata);
    }

    // check should take keyboard focus
    if (next_comp != NULL && (next_comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS) &&
        !(next_comp->flags & XCOMP_FLAG_HAS_KEYBOARD_FOCUS))
    {
        next_comp->flags |= XCOMP_FLAG_HAS_KEYBOARD_FOCUS;
        next_comp->event_handler(next_comp, XCOMP_EVENT_KEYBOARD_FOCUS_GAINED, edata);
    }
}

void xcomp_root_give_next_sibling_keyboard_focus(xcomp_root* root, xcomp_component* comp)
{
    // User is (hopefully) smart enough to give us a component with a valid
    // parent.
    xcomp_component* parent = comp->parent;

    // Find index of comp
    size_t index = 0;
    for (; index < parent->num_children; index++)
    {
        if (parent->children[index] == comp)
            break;
    }

    // Find the next child to the right to give component to
    for (size_t i = index + 1; i < parent->num_children; i++)
    {
        if (parent->children[i]->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
        {
            xcomp_root_give_keyboard_focus(root, parent->children[i]);
            return;
        }
    }

    // If the previous loop failed, there were no valid components to right.
    // Apply the same strategy starting from the begging of the list.
    for (size_t i = 0; i < index; i++)
    {
        if (parent->children[i]->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
        {
            xcomp_root_give_keyboard_focus(root, parent->children[i]);
            break;
        }
    }
}

void xcomp_init(xcomp_component* comp, void* data)
{
    comp->event_handler = &xcomp_empty_event_cb;
    comp->data          = data;
}

void xcomp_empty_event_cb(xcomp_component* comp, uint32_t e, xcomp_event_data d) {}

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
    comp->num_children++;
}

void xcomp_remove_child(xcomp_component* comp, xcomp_component* child)
{
    bool child_was_removed = false;

    size_t i = comp->num_children;

    // Search through nodes and zero the child comp
    for (; i-- != 0;)
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

const xcomp_component* xcomp_get_root_component(const xcomp_component* comp)
{
    const xcomp_component* c = comp;

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
        bool should_skip = child->flags & (XCOMP_FLAG_IS_HIDDEN | XCOMP_FLAG_IS_DISABLED);
        if (!should_skip && xcomp_hit_test(child->dimensions, p))
            return xcomp_find_child_at(child, p);
    }

    return comp;
}

xcomp_component* xcomp_find_parent_at(xcomp_component* comp, xcomp_position p)
{
    if (comp->parent != NULL && !xcomp_hit_test(comp->parent->dimensions, p))
        return xcomp_find_parent_at(comp->parent, p);

    return NULL;
}

void xcomp_send_event_to_children_recursive(xcomp_component* comp, uint32_t ev_type, xcomp_event_data info)
{
    comp->event_handler(comp, ev_type, info);
    for (int i = 0; i < comp->num_children; i++)
        xcomp_send_event_to_children_recursive(comp->children[i], ev_type, info);
}

void xcomp_send_mouse_enter(xcomp_component* comp, xcomp_event_data info)
{
    XCOMP_ASSERT(!(comp->flags & XCOMP_FLAG_IS_MOUSE_OVER));
    comp->flags |= XCOMP_FLAG_IS_MOUSE_OVER;
    comp->event_handler(comp, XCOMP_EVENT_MOUSE_ENTER, info);
    comp->event_handler(comp, XCOMP_EVENT_MOUSE_MOVE, info);
}

void xcomp_send_mouse_exit(xcomp_component* comp, xcomp_event_data info)
{
    XCOMP_ASSERT(comp->flags & XCOMP_FLAG_IS_MOUSE_OVER);
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
        if (xcomp_hit_test(root->main->dimensions, root->position))
        {
            next_over = xcomp_find_child_at(root->main, root->position);

            root->mouse_over = next_over;

            xcomp_send_mouse_enter(next_over, info);
        }
    }
    // Check for drag
    else if (last_over == root->mouse_left_down)
    {
        float distance_x = fabsf(root->mouse_down_pos.x - info.x);
        float distance_y = fabsf(root->mouse_down_pos.y - info.y);
        float distance_r = hypotf(distance_x, distance_y);
        // Check still dragging
        if (last_over->flags & XCOMP_FLAG_IS_DRAGGING)
        {
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_MOVE, info);

            if (last_over->flags & XCOMP_FLAG_CAN_DRAG_AND_DROP)
            {
                next_over = xcomp_find_child_at(root->main, root->position);

                if (next_over != root->mouse_drag_over)
                {
                    xcomp_component* last_drag_over = root->mouse_drag_over;
                    root->mouse_drag_over           = next_over;

                    if (last_drag_over != NULL)
                    {
                        last_drag_over->event_handler(last_drag_over, XCOMP_EVENT_DRAG_EXIT, info);
                    }

                    if (next_over != NULL)
                    {
                        next_over->event_handler(next_over, XCOMP_EVENT_DRAG_ENTER, info);
                    }
                }
            }
        }
        else if (distance_r > 5)
        {
            last_over->flags |= XCOMP_FLAG_IS_DRAGGING;
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_START, info);
            last_over->event_handler(last_over, XCOMP_EVENT_DRAG_MOVE, info);
        }
    }
    else
    {
        XCOMP_ASSERT(last_over->flags & XCOMP_FLAG_IS_MOUSE_OVER);

        // check mouse still over
        if (xcomp_hit_test(last_over->dimensions, root->position))
            next_over = xcomp_find_child_at(last_over, root->position);
        // Check mouse still in window
        // User may have finished a drag
        else if (xcomp_hit_test(root->main->dimensions, root->position))
            next_over = xcomp_find_child_at(root->main, root->position);

        root->mouse_over = next_over;

        // if failed finding child
        if (last_over == next_over)
        {
            XCOMP_ASSERT(next_over->flags & XCOMP_FLAG_IS_MOUSE_OVER);
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
}

void xcomp_send_mouse_down(xcomp_root* root, xcomp_event_data info)
{
    xcomp_position   pos  = {.x = info.x, .y = info.y};
    xcomp_component* comp = xcomp_find_child_at(root->main, pos);
    if (comp != NULL)
    {
        XCOMP_ASSERT(comp->flags & XCOMP_FLAG_IS_MOUSE_OVER);
        if (comp != root->keyboard_focus && root->keyboard_focus != NULL)
        {
            xcomp_component* last_comp  = root->keyboard_focus;
            root->keyboard_focus        = NULL;
            last_comp->flags           &= ~XCOMP_FLAG_HAS_KEYBOARD_FOCUS;
            last_comp->event_handler(last_comp, XCOMP_EVENT_KEYBOARD_FOCUS_LOST, info);
        }

        if (comp->flags & XCOMP_FLAG_WANTS_KEYBOARD_FOCUS)
            xcomp_root_give_keyboard_focus(root, comp);

        // handle left button
        if ((info.modifiers & XCOMP_MOD_LEFT_BUTTON) && root->mouse_left_down == NULL)
        {
            root->mouse_left_down  = comp;
            root->mouse_down_pos.x = info.x;
            root->mouse_down_pos.y = info.y;

            comp->flags |= XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
            comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_DOWN, info);
        }

        // handle right button
        if ((info.modifiers & XCOMP_MOD_RIGHT_BUTTON) && root->mouse_right_down == NULL)
        {
            root->mouse_right_down = comp;

            comp->flags |= XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
            comp->event_handler(comp, XCOMP_EVENT_MOUSE_RIGHT_DOWN, info);
        }

        // handle middle button
        if ((info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) && root->mouse_middle_down == NULL)
        {
            root->mouse_middle_down = comp;

            comp->flags |= XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
            comp->event_handler(comp, XCOMP_EVENT_MOUSE_MIDDLE_DOWN, info);
        }
    }
}

void xcomp_send_mouse_up(xcomp_root* root, xcomp_event_data info, uint32_t time_ms, uint32_t double_click_interval_ms)
{
    xcomp_position   pos  = {.x = info.x, .y = info.y};
    xcomp_component* comp = xcomp_find_child_at(root->main, pos);

    // release left button
    if (root->mouse_left_down != NULL && (info.modifiers & XCOMP_MOD_LEFT_BUTTON) == 0)
    {
        xcomp_component* last_comp = root->mouse_left_down;
        bool             dragging  = !!(last_comp->flags & XCOMP_FLAG_IS_DRAGGING);
        root->mouse_left_down      = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_LEFT_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
            last_comp->event_handler(last_comp, XCOMP_EVENT_MOUSE_LEFT_UP, info);
        }

        if (dragging)
        {
            if (root->mouse_drag_over != NULL)
            {
                xcomp_component* last_drag_over = root->mouse_drag_over;
                root->mouse_drag_over           = NULL;
                last_drag_over->event_handler(last_drag_over, XCOMP_EVENT_DRAG_DROP, info);
            }

            last_comp->flags &= ~XCOMP_FLAG_IS_DRAGGING;
            last_comp->event_handler(last_comp, XCOMP_EVENT_DRAG_END, info);

            if (comp != last_comp && (last_comp->flags & XCOMP_FLAG_IS_MOUSE_OVER))
            {
                XCOMP_ASSERT(root->mouse_over == last_comp);
                root->mouse_over = NULL;
                xcomp_send_mouse_exit(last_comp, info);
                xcomp_send_mouse_position(root, info);
            }
        }

        if (last_comp == comp && !dragging)
        {
            XCOMP_ASSERT(comp);
            uint32_t diff = time_ms - root->last_left_click_time;

            if (root->mouse_last_left_click != last_comp)
                root->left_click_counter = 0;
            if (diff > double_click_interval_ms)
                root->left_click_counter = 0;

            root->left_click_counter++;
            root->last_left_click_time  = time_ms;
            root->mouse_last_left_click = comp;

            uint32_t clicks = root->left_click_counter % 3;
            if (clicks == 0)
                comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_TRIPLE_CLICK, info);
            else if (clicks == 1)
                comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_CLICK, info);
            else if (clicks == 2)
                comp->event_handler(comp, XCOMP_EVENT_MOUSE_LEFT_DOUBLE_CLICK, info);
        }
    }

    // release right button
    if (root->mouse_right_down != NULL && (info.modifiers & XCOMP_MOD_RIGHT_BUTTON) == 0)
    {
        xcomp_component* last_comp = root->mouse_right_down;
        root->mouse_right_down     = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
            last_comp->event_handler(last_comp, XCOMP_EVENT_MOUSE_RIGHT_UP, info);
        }

        if (last_comp == comp)
        {
            // We probably don't care about right double clicks
            last_comp->event_handler(last_comp, XCOMP_EVENT_MOUSE_RIGHT_CLICK, info);
        }
    }

    // release middle button
    if (root->mouse_middle_down != NULL && (info.modifiers & XCOMP_MOD_MIDDLE_BUTTON) == 0)
    {
        xcomp_component* last_comp = root->mouse_middle_down;
        root->mouse_middle_down    = NULL;

        if (last_comp->flags & XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN)
        {
            last_comp->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
            last_comp->event_handler(last_comp, XCOMP_EVENT_MOUSE_MIDDLE_UP, info);
        }

        if (last_comp == comp)
        {
            // We probably don't care about middle double clicks
            last_comp->event_handler(last_comp, XCOMP_EVENT_MOUSE_MIDDLE_CLICK, info);
        }
    }
}

void xcomp_root_clear(xcomp_root* root)
{
    xcomp_component* last_mouse_over        = root->mouse_over;
    xcomp_component* last_mouse_left_down   = root->mouse_left_down;
    xcomp_component* last_mouse_right_down  = root->mouse_right_down;
    xcomp_component* last_mouse_middle_down = root->mouse_middle_down;
    xcomp_component* last_mouse_drag_over   = root->mouse_drag_over;
    xcomp_event_data edata                  = {.x = root->position.x, .y = root->position.y, .modifiers = 0};

    root->mouse_over            = NULL;
    root->mouse_left_down       = NULL;
    root->mouse_last_left_click = NULL;
    root->last_left_click_time  = 0;
    root->left_click_counter    = 0;
    root->mouse_right_down      = NULL;
    root->mouse_middle_down     = NULL;
    root->mouse_drag_over       = NULL;

    xcomp_root_give_keyboard_focus(root, NULL);

    if (last_mouse_drag_over != NULL)
    {
        last_mouse_drag_over->event_handler(last_mouse_drag_over, XCOMP_EVENT_DRAG_EXIT, edata);
    }

    if (last_mouse_middle_down != NULL)
    {
        last_mouse_middle_down->flags &= ~XCOMP_FLAG_IS_MOUSE_MIDDLE_DOWN;
        last_mouse_middle_down->event_handler(last_mouse_middle_down, XCOMP_EVENT_MOUSE_MIDDLE_UP, edata);
    }

    if (last_mouse_right_down != NULL)
    {
        last_mouse_right_down->flags &= ~XCOMP_FLAG_IS_MOUSE_RIGHT_DOWN;
        last_mouse_right_down->event_handler(last_mouse_right_down, XCOMP_EVENT_MOUSE_RIGHT_UP, edata);
    }

    if (last_mouse_left_down != NULL)
    {
        if (last_mouse_left_down->flags & XCOMP_FLAG_IS_DRAGGING)
        {
            last_mouse_left_down->flags &= ~XCOMP_FLAG_IS_DRAGGING;
            last_mouse_left_down->event_handler(last_mouse_left_down, XCOMP_EVENT_DRAG_END, edata);
        }

        last_mouse_left_down->flags &= ~XCOMP_FLAG_IS_MOUSE_LEFT_DOWN;
        last_mouse_left_down->event_handler(last_mouse_left_down, XCOMP_EVENT_MOUSE_LEFT_UP, edata);
    }

    if (last_mouse_over != NULL)
        xcomp_send_mouse_exit(last_mouse_over, edata);
}

void xcomp_root_refresh(xcomp_root* root)
{
    xcomp_root_clear(root);

    // find new component for mouse to be over
    xcomp_event_data data = {.x = root->position.x, .y = root->position.y, .modifiers = 0};
    xcomp_send_mouse_position(root, data);
}

#ifdef __cplusplus
}
#endif

#endif // XHL_COMPONENT_IMPL
