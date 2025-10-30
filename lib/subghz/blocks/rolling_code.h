#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <lib/flipper_format/flipper_format.h>
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rolling code counter state structure
 * Tracks counter state per protocol/device combination
 */
typedef struct {
    uint32_t serial;        // Device serial number (unique identifier)
    uint32_t counter;        // Current counter value
    uint32_t last_sent;     // Last successfully sent counter
    uint32_t max_counter;   // Maximum counter value (protocol-specific, typically 0xFFFF)
    bool is_synced;          // Whether counter is synchronized with device
    uint32_t sync_offset;    // Offset if device counter is ahead
} RollingCodeState;

/**
 * Rolling code manager context
 * Manages counter states for multiple devices/protocols
 */
typedef struct RollingCodeManager RollingCodeManager;

/**
 * Counter increment strategies
 */
typedef enum {
    RollingCodeIncrementFixed,      // Fixed increment (default: +1)
    RollingCodeIncrementVariable,    // Variable increment based on multiplier
    RollingCodeIncrementCustom,      // Custom increment function
    RollingCodeIncrementJump,        // Jump to specific value (for resync)
} RollingCodeIncrementMode;

/**
 * Counter overflow strategies
 */
typedef enum {
    RollingCodeOverflowReset,       // Reset to 0 on overflow
    RollingCodeOverflowWraparound,   // Wrap around (default)
    RollingCodeOverflowSaturate,    // Stay at max value
    RollingCodeOverflowCustom,      // Custom overflow handler
} RollingCodeOverflowMode;

/**
 * Allocate rolling code manager
 * @return RollingCodeManager* pointer to manager instance
 */
RollingCodeManager* rolling_code_manager_alloc(void);

/**
 * Free rolling code manager
 * @param manager Pointer to RollingCodeManager instance
 */
void rolling_code_manager_free(RollingCodeManager* manager);

/**
 * Register a device/protocol combination
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param initial_counter Initial counter value
 * @param max_counter Maximum counter value (0xFFFF for 16-bit, 0xFFFFFFFF for 32-bit)
 * @return true if successful
 */
bool rolling_code_manager_register_device(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t initial_counter,
    uint32_t max_counter);

/**
 * Get current counter for a device
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param counter_out Output pointer for counter value
 * @return true if device found and counter retrieved
 */
bool rolling_code_manager_get_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t* counter_out);

/**
 * Increment counter for a device with advanced options
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param increment Amount to increment (uses multiplier if 0)
 * @param mode Increment mode
 * @param new_counter_out Output pointer for new counter value
 * @return true if successful
 */
bool rolling_code_manager_increment_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t increment,
    RollingCodeIncrementMode mode,
    uint32_t* new_counter_out);

/**
 * Set counter to specific value (for resynchronization)
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param counter New counter value
 * @param mark_synced Whether to mark as synchronized
 * @return true if successful
 */
bool rolling_code_manager_set_counter(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t counter,
    bool mark_synced);

/**
 * Get counter state (full state information)
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param state_out Output pointer for state structure
 * @return true if device found
 */
bool rolling_code_manager_get_state(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    RollingCodeState* state_out);

/**
 * Check if counter is synchronized with device
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @return true if synchronized
 */
bool rolling_code_manager_is_synced(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial);

/**
 * Mark counter as synchronized
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 */
void rolling_code_manager_mark_synced(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial);

/**
 * Calculate optimal counter value to resync with device
 * Handles cases where device counter is ahead
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param device_counter Counter value observed from device
 * @param suggested_counter_out Output pointer for suggested counter
 * @return true if resync calculation successful
 */
bool rolling_code_manager_calculate_resync(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    uint32_t device_counter,
    uint32_t* suggested_counter_out);

/**
 * Handle counter overflow with strategy
 * @param manager Pointer to RollingCodeManager instance
 * @param protocol_name Protocol name identifier
 * @param serial Device serial number
 * @param mode Overflow handling mode
 * @param new_counter_out Output pointer for counter after overflow handling
 * @return true if overflow was handled
 */
bool rolling_code_manager_handle_overflow(
    RollingCodeManager* manager,
    const char* protocol_name,
    uint32_t serial,
    RollingCodeOverflowMode mode,
    uint32_t* new_counter_out);

/**
 * Save counter states to file
 * @param manager Pointer to RollingCodeManager instance
 * @param flipper_format Pointer to FlipperFormat instance
 * @return true if successful
 */
bool rolling_code_manager_save(RollingCodeManager* manager, FlipperFormat* flipper_format);

/**
 * Load counter states from file
 * @param manager Pointer to RollingCodeManager instance
 * @param flipper_format Pointer to FlipperFormat instance
 * @return true if successful
 */
bool rolling_code_manager_load(RollingCodeManager* manager, FlipperFormat* flipper_format);

/**
 * Universal counter increment function
 * Uses global multiplier setting with advanced overflow handling
 * @param current_counter Current counter value
 * @param multiplier Increment multiplier (from furi_hal_subghz_get_rolling_counter_mult)
 * @param max_counter Maximum counter value
 * @param use_ofex Whether to use OFEX (overflow experimental) mode
 * @param new_counter_out Output pointer for new counter value
 * @return true if successful
 */
bool rolling_code_increment_universal(
    uint32_t current_counter,
    int32_t multiplier,
    uint32_t max_counter,
    bool use_ofex,
    uint32_t* new_counter_out);

/**
 * Smart counter increment with protocol-aware logic
 * Handles different increment patterns for various protocols
 * @param current_counter Current counter value
 * @param increment Amount to increment
 * @param multiplier Global multiplier setting
 * @param max_counter Maximum counter value
 * @param protocol_type Protocol type identifier (for special handling)
 * @param new_counter_out Output pointer for new counter value
 * @return true if successful
 */
bool rolling_code_increment_smart(
    uint32_t current_counter,
    uint32_t increment,
    int32_t multiplier,
    uint32_t max_counter,
    const char* protocol_type,
    uint32_t* new_counter_out);

#ifdef __cplusplus
}
#endif
