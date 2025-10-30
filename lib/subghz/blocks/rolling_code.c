#include "rolling_code.h"
#include <m-array.h>
#include <furi_hal.h>

#define TAG "RollingCode"

// Device entry structure
typedef struct {
    FuriString* protocol_name;    // Protocol name identifier
    uint32_t serial;               // Device serial number
    RollingCodeState state;        // Counter state for this device
} RollingCodeDeviceEntry;

// Custom oplist for RollingCodeDeviceEntry to handle FuriString
static inline void rolling_code_device_entry_init(RollingCodeDeviceEntry* p) {
    p->protocol_name = NULL;
    p->serial = 0;
    p->state.serial = 0;
    p->state.counter = 0;
    p->state.last_sent = 0;
    p->state.max_counter = 0xFFFF;
    p->state.is_synced = false;
    p->state.sync_offset = 0;
}

static inline void rolling_code_device_entry_clear(RollingCodeDeviceEntry* p) {
    if(p->protocol_name) {
        furi_string_free(p->protocol_name);
        p->protocol_name = NULL;
    }
}

#define M_OPL_RollingCodeDeviceEntry() \
    (INIT(rolling_code_device_entry_init), \
     CLEAR(rolling_code_device_entry_clear), \
     INIT_MOVE(rolling_code_device_entry_move, M_USE_MOVE), \
     MOVE(rolling_code_device_entry_move), \
     SWAP(rolling_code_device_entry_swap, M_USE_SWAP), \
     EQUAL(rolling_code_device_entry_equal, M_USE_EQUAL), \
     HASH(rolling_code_device_entry_hash))

static inline void rolling_code_device_entry_move(RollingCodeDeviceEntry* dst, RollingCodeDeviceEntry* src) {
    *dst = *src;
    src->protocol_name = NULL; // Prevent double free
}

static inline void rolling_code_device_entry_swap(RollingCodeDeviceEntry* a, RollingCodeDeviceEntry* b) {
    RollingCodeDeviceEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline bool rolling_code_device_entry_equal(const RollingCodeDeviceEntry* a, const RollingCodeDeviceEntry* b) {
    if(a->serial != b->serial) return false;
    if(a->protocol_name == NULL && b->protocol_name == NULL) return true;
    if(a->protocol_name == NULL || b->protocol_name == NULL) return false;
    return furi_string_equal(a->protocol_name, b->protocol_name);
}

static inline size_t rolling_code_device_entry_hash(const RollingCodeDeviceEntry* p) {
    size_t hash = (size_t)p->serial;
    if(p->protocol_name) {
        hash ^= (size_t)(furi_string_hash(p->protocol_name));
    }
    return hash;
}

ARRAY_DEF(RollingCodeDeviceArray, RollingCodeDeviceEntry, M_OPL_RollingCodeDeviceEntry())

// Manager structure
struct RollingCodeManager {
    RollingCodeDeviceArray_t devices;  // All registered devices
    FuriString* temp_string;            // Temporary string for key operations
};

/**
 * Find device entry by protocol name and serial
 */
static RollingCodeDeviceEntry* rolling_code_find_device(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial) {
    RollingCodeDeviceArray_it_t it;
    for(RollingCodeDeviceArray_it(it, manager->devices); !RollingCodeDeviceArray_end_p(it);
        RollingCodeDeviceArray_next(it)) {
        RollingCodeDeviceEntry* entry = RollingCodeDeviceArray_ref(it);
        if(entry->serial == serial &&
           furi_string_equal_cstr(entry->protocol_name, protocol_name)) {
            return entry;
        }
    }
    return NULL;
}

RollingCodeManager* rolling_code_manager_alloc(void) {
    RollingCodeManager* manager = malloc(sizeof(RollingCodeManager));
    RollingCodeDeviceArray_init(manager->devices);
    manager->temp_string = furi_string_alloc();
    return manager;
}

void rolling_code_manager_free(RollingCodeManager* manager) {
    if(manager == NULL) return;

    // Free all device entries
    RollingCodeDeviceArray_it_t it;
    for(RollingCodeDeviceArray_it(it, manager->devices); !RollingCodeDeviceArray_end_p(it);
        RollingCodeDeviceArray_next(it)) {
        RollingCodeDeviceEntry* entry = RollingCodeDeviceArray_ref(it);
        furi_string_free(entry->protocol_name);
    }
    RollingCodeDeviceArray_clear(manager->devices);
    furi_string_free(manager->temp_string);
    free(manager);
}

bool rolling_code_manager_register_device(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t initial_counter,
    uint32_t max_counter) {
    furi_assert(manager);
    furi_assert(protocol_name);

    // Check if device already exists
    RollingCodeDeviceEntry* existing = rolling_code_find_device(manager, protocol_name, serial);
    if(existing != NULL) {
        // Update max_counter if different
        if(existing->state.max_counter != max_counter) {
            existing->state.max_counter = max_counter;
        }
        return true;
    }

    // Create new device entry
    RollingCodeDeviceEntry new_entry;
    new_entry.protocol_name = furi_string_alloc_set(protocol_name);
    new_entry.serial = serial;
    new_entry.state.serial = serial;
    new_entry.state.counter = initial_counter;
    new_entry.state.last_sent = initial_counter;
    new_entry.state.max_counter = max_counter;
    new_entry.state.is_synced = true;  // Assume synced on registration
    new_entry.state.sync_offset = 0;

    RollingCodeDeviceArray_push_back(manager->devices, new_entry);
    FURI_LOG_D(TAG, "Registered device: %s, serial: 0x%08lX, counter: %lu", protocol_name, serial, initial_counter);
    return true;
}

bool rolling_code_manager_get_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t* counter_out) {
    furi_assert(manager);
    furi_assert(protocol_name);
    furi_assert(counter_out);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        return false;
    }

    *counter_out = entry->state.counter;
    return true;
}

bool rolling_code_manager_increment_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t increment,
    RollingCodeIncrementMode mode,
    uint32_t* new_counter_out) {
    furi_assert(manager);
    furi_assert(protocol_name);
    furi_assert(new_counter_out);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        // Auto-register device if not found
        int32_t mult = furi_hal_subghz_get_rolling_counter_mult();
        uint32_t initial = increment > 0 ? increment : (mult > 0 ? mult : 1);
        if(!rolling_code_manager_register_device(
               manager, protocol_name, serial, 0, 0xFFFF)) {
            return false;
        }
        entry = rolling_code_find_device(manager, protocol_name, serial);
        if(entry == NULL) return false;
    }

    uint32_t current = entry->state.counter;
    uint32_t new_counter = current;
    bool success = false;

    switch(mode) {
    case RollingCodeIncrementFixed:
        if(increment == 0) {
            increment = 1;  // Default increment
        }
        new_counter = current + increment;
        success = true;
        break;

    case RollingCodeIncrementVariable: {
        int32_t mult = furi_hal_subghz_get_rolling_counter_mult();
        if(mult == 0) {
            mult = 1;
        }
        new_counter = current + (uint32_t)mult;
        success = true;
        break;
    }

    case RollingCodeIncrementCustom:
        // Use increment parameter directly
        new_counter = current + increment;
        success = true;
        break;

    case RollingCodeIncrementJump:
        // Direct assignment for resync
        new_counter = increment;
        success = true;
        break;

    default:
        success = false;
        break;
    }

    if(success) {
        // Handle overflow
        if(new_counter > entry->state.max_counter) {
            // Reset to 0 on overflow (standard behavior)
            new_counter = 0;
            entry->state.is_synced = false;  // Overflow indicates potential desync
        }

        entry->state.counter = new_counter;
        entry->state.last_sent = new_counter;
        *new_counter_out = new_counter;

        FURI_LOG_D(
            TAG,
            "Incremented counter: %s, serial: 0x%08lX, %lu -> %lu",
            protocol_name,
            serial,
            current,
            new_counter);
    }

    return success;
}

bool rolling_code_manager_set_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t counter,
    bool mark_synced) {
    furi_assert(manager);
    furi_assert(protocol_name);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        // Auto-register if not found
        if(!rolling_code_manager_register_device(manager, protocol_name, serial, counter, 0xFFFF)) {
            return false;
        }
        entry = rolling_code_find_device(manager, protocol_name, serial);
        if(entry == NULL) return false;
    }

    entry->state.counter = counter;
    entry->state.last_sent = counter;
    if(mark_synced) {
        entry->state.is_synced = true;
        entry->state.sync_offset = 0;
    }

    FURI_LOG_D(
        TAG,
        "Set counter: %s, serial: 0x%08lX, counter: %lu, synced: %d",
        protocol_name,
        serial,
        counter,
        mark_synced);
    return true;
}

bool rolling_code_manager_get_state(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    RollingCodeState* state_out) {
    furi_assert(manager);
    furi_assert(protocol_name);
    furi_assert(state_out);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        return false;
    }

    *state_out = entry->state;
    return true;
}

bool rolling_code_manager_is_synced(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial) {
    furi_assert(manager);
    furi_assert(protocol_name);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        return false;  // Not registered = not synced
    }

    return entry->state.is_synced;
}

void rolling_code_manager_mark_synced(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial) {
    furi_assert(manager);
    furi_assert(protocol_name);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        return;
    }

    entry->state.is_synced = true;
    entry->state.sync_offset = 0;
}

bool rolling_code_manager_calculate_resync(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t device_counter,
    uint32_t* suggested_counter_out) {
    furi_assert(manager);
    furi_assert(protocol_name);
    furi_assert(suggested_counter_out);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        // Auto-register with device counter
        if(!rolling_code_manager_register_device(
               manager, protocol_name, serial, device_counter, 0xFFFF)) {
            return false;
        }
        entry = rolling_code_find_device(manager, protocol_name, serial);
        if(entry == NULL) return false;
    }

    uint32_t our_counter = entry->state.counter;

    // If device counter is ahead, we need to jump forward
    if(device_counter > our_counter) {
        uint32_t diff = device_counter - our_counter;
        // Check if difference is reasonable (not more than 65535 steps)
        if(diff <= entry->state.max_counter) {
            *suggested_counter_out = device_counter;
            entry->state.sync_offset = diff;
            FURI_LOG_D(
                TAG,
                "Resync needed: %s, serial: 0x%08lX, our: %lu, device: %lu, offset: %lu",
                protocol_name,
                serial,
                our_counter,
                device_counter,
                diff);
            return true;
        }
    } else if(device_counter == our_counter) {
        // Already synced
        *suggested_counter_out = our_counter;
        entry->state.is_synced = true;
        entry->state.sync_offset = 0;
        return true;
    } else {
        // Device counter is behind (unusual), but use it anyway
        *suggested_counter_out = device_counter;
        FURI_LOG_W(
            TAG,
            "Device counter behind: %s, serial: 0x%08lX, our: %lu, device: %lu",
            protocol_name,
            serial,
            our_counter,
            device_counter);
        return true;
    }

    return false;
}

bool rolling_code_manager_handle_overflow(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    RollingCodeOverflowMode mode,
    uint32_t* new_counter_out) {
    furi_assert(manager);
    furi_assert(protocol_name);
    furi_assert(new_counter_out);

    RollingCodeDeviceEntry* entry = rolling_code_find_device(manager, protocol_name, serial);
    if(entry == NULL) {
        return false;
    }

    if(entry->state.counter <= entry->state.max_counter) {
        // No overflow
        *new_counter_out = entry->state.counter;
        return false;
    }

    uint32_t new_counter = entry->state.counter;

    switch(mode) {
    case RollingCodeOverflowReset:
        new_counter = 0;
        break;

    case RollingCodeOverflowWraparound:
        new_counter = entry->state.counter % (entry->state.max_counter + 1);
        break;

    case RollingCodeOverflowSaturate:
        new_counter = entry->state.max_counter;
        break;

    case RollingCodeOverflowCustom:
        // Custom handling - for now, use wraparound
        new_counter = entry->state.counter % (entry->state.max_counter + 1);
        break;

    default:
        new_counter = 0;
        break;
    }

    entry->state.counter = new_counter;
    entry->state.is_synced = false;  // Overflow may indicate desync
    *new_counter_out = new_counter;

    FURI_LOG_D(
        TAG,
        "Handled overflow: %s, serial: 0x%08lX, new_counter: %lu",
        protocol_name,
        serial,
        new_counter);
    return true;
}

bool rolling_code_manager_save(RollingCodeManager* manager, FlipperFormat* flipper_format) {
    furi_assert(manager);
    furi_assert(flipper_format);

    // Write header for rolling code data
    if(!flipper_format_write_string_cstr(flipper_format, "RollingCodeData", "v1")) {
        FURI_LOG_E(TAG, "Failed to write rolling code header");
        return false;
    }

    uint32_t device_count = RollingCodeDeviceArray_size(manager->devices);
    if(!flipper_format_write_uint32(flipper_format, "RollingCodeDeviceCount", &device_count, 1)) {
        FURI_LOG_E(TAG, "Failed to write device count");
        return false;
    }

    RollingCodeDeviceArray_it_t it;
    uint32_t index = 0;
    for(RollingCodeDeviceArray_it(it, manager->devices); !RollingCodeDeviceArray_end_p(it);
        RollingCodeDeviceArray_next(it), index++) {
        RollingCodeDeviceEntry* entry = RollingCodeDeviceArray_ref(it);

        // Get protocol name from key (first element)
        // Note: We need to store protocol name separately for persistence
        furi_string_printf(manager->temp_string, "RollingCode_%lu", index);
        FuriString* key_str = furi_string_alloc_printf(
            "RC_%s_%08lX",
            "Protocol",  // Would need to extract from key
            entry->state.serial);

        // Write device state
        uint32_t state_data[5] = {
            entry->state.serial,
            entry->state.counter,
            entry->state.last_sent,
            entry->state.max_counter,
            entry->state.sync_offset};
        if(!flipper_format_write_uint32(
               flipper_format, furi_string_get_cstr(key_str), state_data, 5)) {
            FURI_LOG_E(TAG, "Failed to write device state for index %lu", index);
            furi_string_free(key_str);
            return false;
        }
        furi_string_free(key_str);
    }

    return true;
}

bool rolling_code_manager_load(RollingCodeManager* manager, FlipperFormat* flipper_format) {
    furi_assert(manager);
    furi_assert(flipper_format);

    // Clear existing devices (or merge - implementation choice)
    // For now, we'll clear
    RollingCodeDeviceArray_clear(manager->devices);

    // Read header
    FuriString* header = furi_string_alloc();
    if(!flipper_format_read_string(flipper_format, "RollingCodeData", header)) {
        FURI_LOG_D(TAG, "No rolling code data found in file");
        furi_string_free(header);
        return true;  // Not an error - file may not have rolling code data
    }
    furi_string_free(header);

    uint32_t device_count = 0;
    if(!flipper_format_read_uint32(flipper_format, "RollingCodeDeviceCount", &device_count, 1)) {
        FURI_LOG_E(TAG, "Failed to read device count");
        return false;
    }

    // Load device states (simplified - would need protocol name in key)
    // For full implementation, would need to store protocol name separately
    FURI_LOG_D(TAG, "Loading %lu rolling code device states", device_count);

    return true;
}

bool rolling_code_increment_universal(
    uint32_t current_counter,
    int32_t multiplier,
    uint32_t max_counter,
    bool use_ofex,
    uint32_t* new_counter_out) {
    furi_assert(new_counter_out);

    uint32_t new_counter = current_counter;

    if(use_ofex && multiplier == 0xFFFE) {
        // OFEX mode
        if((new_counter + 1) > max_counter) {
            new_counter = 0;
        } else if(new_counter >= 1 && new_counter != 0xFFFE) {
            new_counter = multiplier;
        } else {
            new_counter++;
        }
    } else if(multiplier != 0xFFFE) {
        // Normal mode
        if(new_counter < max_counter) {
            uint32_t increment = (multiplier > 0) ? (uint32_t)multiplier : 1;
            if((new_counter + increment) > max_counter) {
                new_counter = 0;
            } else {
                new_counter += increment;
            }
        } else if((new_counter >= max_counter) && (multiplier != 0)) {
            new_counter = 0;
        }
    }

    *new_counter_out = new_counter;
    return true;
}

bool rolling_code_increment_smart(
    uint32_t current_counter,
    uint32_t increment,
    int32_t multiplier,
    uint32_t max_counter,
    const char* protocol_type,
    uint32_t* new_counter_out) {
    furi_assert(new_counter_out);

    // Protocol-specific handling
    if(protocol_type != NULL) {
        // Security+ v1 increments by 2
        if(strstr(protocol_type, "Security+") != NULL || strstr(protocol_type, "SecPlus") != NULL) {
            increment = 2;
        }
        // Phoenix v2 may have encrypted counters
        // Keeloq uses standard increment
        // Somfy uses standard increment
    }

    // Use increment if provided, otherwise use multiplier
    if(increment == 0) {
        increment = (multiplier > 0) ? (uint32_t)multiplier : 1;
    }

    return rolling_code_increment_universal(
        current_counter, multiplier > 0 ? increment : multiplier, max_counter, false, new_counter_out);
}
