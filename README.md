# xhl

Stands for:

-   xingle header libraries
-   xtendable header libraries
-   sexy header libraries
-   extreme header libraries

[STB-Style](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt) headers

### [array.h](include/xhl/array.h)

Dynamic array, type agnostic. Based on [stb_ds.h](https://github.com/nothings/stb/blob/master/stb_ds.h) by Sean Barrett.

Supports custom allocator. Relloc calls are inlined in case you need to track where they're called from.

```c
#define XARR_REALLOC(ptr, size) (printf("realloc: %p, (%uz) - %s:%d\n", (ptr), (size), __FILE__, __LINE__), realloc(ptr, size))
#define XARR_FREE(ptr) (printf("free: %p - %s:%d\n", p, __FILE__, __LINE__), free(ptr))
#include <xhl/array.h>

int* nums = NULL;
for (int i = 0; i < 10; i++)
    xarr_push(nums, 1);
xarr_delete(nums, 8);
xarr_delete(nums, 5);
xarr_insert(nums, 2, 69);
xarr_free(nums);
```

### [component.h](include/xhl/component.h)

Backbone for a retained mode GUI. Contains base widget struct and all of the logic for sending mouse and keyboard events from your to components in your heirarchy. Use this to write your own widgets which respond to events. Pair with your desired graphics engine.

Base component is optimised to be very small.

```c
// gui.c
struct GUI {
    xcomp_root root;
    xcomp_component top;
    xcomp_component* children[1];
    xcomp_component child_btn1;
}
void handle_events_btn1(struct xcomp_component* comp uint32_t e, xcomp_event_data)
{
    if (e == XCOMP_EVENT_MOUSE_ENTER)
    {
        GUI* gui = (GUI*)comp->data;
        ... // eg. change mouse cursor
    }
}
void init_gui(GUI* gui)
{
    gui->root.main = &gui->top;
    // Fixed size array, avoids additional allocations
    gui->root.top.children = gui->children;
    xcomp_add_child(&gui->root.top, &gui->root.child_btn1);
    gui->child_btn1.event_handler = handle_events_btn1;
    gui->child_btn1.data = gui;
}
// os_window_events.c
...
case MOUSE_MOVE:
    xcomp_send_mouse_position(&gui->root, x, y);
case MOUSE_DOWN:
    xcomp_send_mouse_down(&gui->root, x, y);
case MOUSE_UP:
    xcomp_send_mouse_up(&gui->root, x, y);
... // etc
```

### [debug.h](include/xhl/debug.h)

Simple macros for pausing your debugger.

```c
xassert(2 + 2 == 5); // pause
```

### [malloc.h](include/xhl/malloc.h)

Throw _"safety"_ to the wind with **xmalloc**! Wraps `malloc`, calling `exit(ENOMEM)` if `NULL` is returned. Additionally tracks number of `xmalloc` calls in `DEBUG` to help spot memory leaks.

```c
#include <xhl/malloc.h>
int main() {
    // will exit with ENOMEM
    int* nums = xmalloc(1U << 63);
    // unreachable
    nums = xrealloc(nums, 2);
    xfree(nums);
    return 0;
}
```
