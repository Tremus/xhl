#ifndef XHL_COMPONENT_H
#define XHL_COMPONENT_H

/**
 * Quick and dirty component heirachy.
 * Should be easily extendable
 * No allocations or includes
 */

enum xcomp_event {
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

enum xcomp_flag {
    XCOMP_FLAG_IS_HIDDEN = 1 << 0,
    XCOMP_FLAG_IS_MOUSE_OVER = 1 << 1,
    XCOMP_FLAG_IS_MOUSE_DOWN = 1 << 2,
    XCOMP_FLAG_IS_DRAGGING = 1 << 3,
    XCOMP_FLAG_WANTS_KEYBOARD_FOCUS = 1 << 4,
};

union xcomp_position {
    struct {
        float x;
        float y;
    };
    float data[2]; // vec2
};

union xcomp_rectangle {
    struct {
        float x;
        float y;
        float width;
        float height;
    };
    float data[4]; // vec4
};

union xcomp_event_data {
    unsigned long raw;
    xcomp_position position;
};

struct xcomp_component {
    xcomp_component* parent;
    xcomp_component** children;
    unsigned int num_children;
    unsigned int cap_children;

    xcomp_rectangle dimension;

    unsigned long flags;
    bool (*handle_event)(xcomp_component*,
                         unsigned int event,
                         xcomp_event_data data);
};

struct xcomp_root {
    xcomp_component* main; // root level component
    xcomp_component* mouse_over;
    xcomp_component* keyboard_focus;
};

// COMPONENT METHODS

// set x/y/w/h on component, then send event
void xcomp_set_dimension(xcomp_component* comp, xcomp_rectangle dimension);

void xcomp_set_position(xcomp_component* comp, float x, float y);
void xcomp_set_size(xcomp_component* comp, float width, float height);

// resizes num children
void xcomp_add_child(xcomp_component* comp, xcomp_component* child);
// resizes num children
void xcomp_remove_child(xcomp_component* comp, xcomp_component* child);
// set flag, send event
void xcomp_set_visible(xcomp_component* comp, bool visible);
// checks through parent heirarchy until it finds the root
xcomp_component* xcomp_get_root_component(xcomp_component* comp);
// returns 0/1 dependon on weather the coordinate lies within component
// dimensions
bool xcomp_hit_test(xcomp_component*, float x, float y);

// recursively loops though children until it finds the bottom level component
// with the given coordinates
xcomp_component* xcomp_find_child_at(xcomp_component*, float x, float y);

// recursively looks though parent heirachy until it finds top level component
// with the given coordinates. May return NULL if coordinate is outside of
// screen dimensions
xcomp_component* xcomp_find_parent_at(xcomp_component*, float x, float y);

// APP METHODS
void xcomp_send_mouse_message(xcomp_root*,
                              float x,
                              float y,
                              xcomp_event_data info);
void xcomp_send_keyboard_message(xcomp_root*,
                                 unsigned int charcode,
                                 xcomp_event_data info);

#ifdef XHL_COMPONENT_IMPL

void xcomp_set_dimension(
    xcomp_component* comp, float x, float y, float width, float height) {
    comp->dimension.x = x;
    comp->dimension.y = y;
    comp->dimension.width = width;
    comp->dimension.height = height;
    xcomp_event_data data = {.raw = 0ul};
    comp->handle_event(comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->handle_event(comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->handle_event(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_position(xcomp_component* comp, float x, float y) {
    comp->dimension.x = x;
    comp->dimension.y = y;
    xcomp_event_data data = {.raw = 0ul};
    comp->handle_event(comp, XCOMP_EVENT_POSITION_CHANGED, data);
    comp->handle_event(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_set_size(xcomp_component* comp, float width, float height) {
    comp->dimension.width = width;
    comp->dimension.height = height;
    xcomp_event_data data = {.raw = 0ul};
    comp->handle_event(comp, XCOMP_EVENT_SIZE_CHANGED, data);
    comp->handle_event(comp, XCOMP_EVENT_DIMENSION_CHANGED, data);
}

void xcomp_add_child(xcomp_component* comp, xcomp_component* child) {
    comp->children[comp->num_children] = child;
    child->parent = comp;
    comp->num_children += 1;
}

void xcomp_remove_child(xcomp_component* comp, xcomp_component* child) {
    bool child_was_removed = false;
    unsigned int i = 0;

    // Search through nodes an zero the child comp
    for (; i < comp->num_children; i++) {
        // try and remove child
        if (comp->children[i] == child) {
            child->parent = 0;
            comp->children[i] = 0;
            child_was_removed = true;
            break;
        }
    }
    if (child_was_removed) {
        // shuffle children back one place
        i += 1;
        for (; i < comp->num_children; i++) {
            comp->children[i - 1] = comp->children[i];
        }

        comp->num_children -= 1;
    }
}

void xcomp_set_visible(xcomp_component* comp, bool visible) {
    if (visible) {
        comp->flags &= ~XCOMP_FLAG_IS_HIDDEN;
    } else {
        comp->flags |= XCOMP_FLAG_IS_HIDDEN;
    }
    xcomp_event_data data = {.raw = visible};
    comp->handle_event(comp, XCOMP_EVENT_VISIBILITY_CHANGED, data);
}

xcomp_component* xcomp_get_root_component(xcomp_component* comp) {
    xcomp_component* c = comp;
    while (c->parent != 0)
        c = c->parent;
    return c;
}

bool xcomp_hit_test(xcomp_component* comp, float x, float y) {
    const float right = comp->dimension.x + comp->dimension.width;
    const float bottom = comp->dimension.y + comp->dimension.height;
    return x >= comp->dimension.x && y >= comp->dimension.y && x <= right
        && y <= bottom;
}

xcomp_component* xcomp_find_child_at(xcomp_component* comp, float x, float y) {
    for (unsigned int i = comp->num_children - 1; i >= 0; i--) {
        if (xcomp_hit_test(comp->children[i], x, y))
            return xcomp_find_child_at(comp->children[i], x, y);
    }
    return comp;
}

xcomp_component* xcomp_find_parent_at(xcomp_component* comp, float x, float y) {
    if (comp->parent != 0 && !xcomp_hit_test(comp->parent, x, y))
        return xcomp_find_parent_at(comp->parent, x, y);
    return 0;
}

void xcomp_send_mouse_message(xcomp_root* root,
                              float x,
                              float y,
                              xcomp_event_data info) {
    if (root->mouse_over == 0) {
        root->mouse_over = xcomp_find_child_at(root->main, x, y);
        root->mouse_over->flags |= XCOMP_FLAG_IS_MOUSE_OVER;
        root->mouse_over->handle_event(
            root->mouse_over, XCOMP_EVENT_MOUSE_ENTER, info);
        root->mouse_over->handle_event(
            root->mouse_over, XCOMP_EVENT_MOUSE_MOVE, info);
    } else {
        xcomp_component* last_over = root->mouse_over;
        root->mouse_over = xcomp_find_child_at(last_over, x, y);
        if (last_over == root->mouse_over) {
            root->mouse_over->handle_event(
                root->mouse_over, XCOMP_EVENT_MOUSE_MOVE, info);
        } else {
            last_over->handle_event(last_over, XCOMP_EVENT_MOUSE_EXIT, info);
            root->mouse_over->handle_event(
                root->mouse_over, XCOMP_EVENT_MOUSE_ENTER, info);
            root->mouse_over->handle_event(
                root->mouse_over, XCOMP_EVENT_MOUSE_MOVE, info);
        }
    }
    // TODO: handle mouse down
    // TODO: handle mouse wheel
    // TODO: handle mouse pinch
}

void xcomp_send_keyboard_message(xcomp_root*,
                                 unsigned int charcode,
                                 xcomp_event_data info) {
    // TODO
}

#endif // XHL_COMPONENT_IMPL
#endif // XHL_COMPONENT_H
