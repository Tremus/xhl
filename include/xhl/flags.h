#pragma once

inline void xf_set_flag(unsigned long* flags, unsigned long flag_to_set) {
    *flags |= flag_to_set;
}

inline bool xf_has_flag(unsigned long flags, unsigned long flags_to_check) {
    return (flags & flags_to_check) > 0;
}

inline void xf_remove_flag(unsigned long* flags, unsigned long flag_to_remove) {
    *flags &= ~flag_to_remove;
}

inline void xf_toggle_flag(unsigned long* flags, unsigned long flag_to_toggle) {
    *flags ^= flag_to_toggle;
}
