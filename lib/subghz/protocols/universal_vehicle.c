#include "universal_vehicle.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "SubGhzProtocolUniVehicle"

// Universal vehicle decoder attempts to decode unknown vehicle key fobs
// using pattern recognition and multiple decoding strategies

static const SubGhzBlockConst subghz_protocol_universal_vehicle_const = {
    .te_short = 200,
    .te_long = 400,
    .te_delta = 150,
    .min_count_bit_for_found = 24, // Minimum bits to attempt decode
};

struct SubGhzProtocolDecoderUniversalVehicle {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint32_t pattern_analysis[16]; // Store timing patterns
    uint8_t pattern_index;
    bool potential_protocol_found;
};

typedef enum {
    UniversalVehicleDecoderStepReset = 0,
    UniversalVehicleDecoderStepAnalyzePattern,
    UniversalVehicleDecoderStepDecode,
    UniversalVehicleDecoderStepSaveDuration,
    UniversalVehicleDecoderStepCheckDuration,
} UniversalVehicleDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_universal_vehicle_decoder = {
    .alloc = subghz_protocol_decoder_universal_vehicle_alloc,
    .free = subghz_protocol_decoder_universal_vehicle_free,

    .feed = subghz_protocol_decoder_universal_vehicle_feed,
    .reset = subghz_protocol_decoder_universal_vehicle_reset,

    .get_hash_data = subghz_protocol_decoder_universal_vehicle_get_hash_data,
    .serialize = subghz_protocol_decoder_universal_vehicle_serialize,
    .deserialize = subghz_protocol_decoder_universal_vehicle_deserialize,
    .get_string = subghz_protocol_decoder_universal_vehicle_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_universal_vehicle_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol subghz_protocol_universal_vehicle = {
    .name = SUBGHZ_PROTOCOL_UNIVERSAL_VEHICLE_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Cars,

    .decoder = &subghz_protocol_universal_vehicle_decoder,
    .encoder = &subghz_protocol_universal_vehicle_encoder,
};

void* subghz_protocol_decoder_universal_vehicle_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderUniversalVehicle* instance =
        malloc(sizeof(SubGhzProtocolDecoderUniversalVehicle));
    instance->base.protocol = &subghz_protocol_universal_vehicle;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->pattern_index = 0;
    instance->potential_protocol_found = false;

    return instance;
}

void subghz_protocol_decoder_universal_vehicle_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;
    free(instance);
}

void subghz_protocol_decoder_universal_vehicle_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;
    instance->decoder.parser_step = UniversalVehicleDecoderStepReset;
    instance->pattern_index = 0;
    instance->potential_protocol_found = false;
}

/**
 * Analyze timing patterns to detect protocol characteristics
 */
static void analyze_timing_pattern(
    SubGhzProtocolDecoderUniversalVehicle* instance,
    uint32_t duration) {
    if(instance->pattern_index < 16) {
        instance->pattern_analysis[instance->pattern_index] = duration;
        instance->pattern_index++;
    }
}

/**
 * Attempt to detect protocol from timing patterns
 */
static bool detect_protocol_from_pattern(SubGhzProtocolDecoderUniversalVehicle* instance) {
    if(instance->pattern_index < 8) return false;

    // Calculate average short and long pulse durations
    uint32_t sum_short = 0, sum_long = 0;
    uint8_t count_short = 0, count_long = 0;

    for(uint8_t i = 0; i < instance->pattern_index; i++) {
        uint32_t dur = instance->pattern_analysis[i];
        if(dur < 500) {
            sum_short += dur;
            count_short++;
        } else if(dur < 2000) {
            sum_long += dur;
            count_long++;
        }
    }

    if(count_short > 0 && count_long > 0) {
        uint32_t avg_short = sum_short / count_short;
        uint32_t avg_long = sum_long / count_long;

        // Check if we have a reasonable 2-level encoding (OOK/ASK)
        if(avg_long > avg_short && avg_long < (avg_short * 4)) {
            instance->potential_protocol_found = true;
            instance->decoder.parser_step = UniversalVehicleDecoderStepDecode;
            return true;
        }
    }

    return false;
}

void subghz_protocol_decoder_universal_vehicle_feed(
    void* context,
    bool level,
    uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;

    switch(instance->decoder.parser_step) {
    case UniversalVehicleDecoderStepReset:
        // Look for potential signal start
        if(level && duration > 100 && duration < 5000) {
            instance->decoder.parser_step = UniversalVehicleDecoderStepAnalyzePattern;
            analyze_timing_pattern(instance, duration);
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
        }
        break;

    case UniversalVehicleDecoderStepAnalyzePattern:
        analyze_timing_pattern(instance, duration);
        if(detect_protocol_from_pattern(instance)) {
            // Protocol pattern detected, switch to decode mode
            break;
        }
        if(instance->pattern_index >= 16) {
            // Enough samples, try to decode anyway
            instance->decoder.parser_step = UniversalVehicleDecoderStepDecode;
        }
        break;

    case UniversalVehicleDecoderStepDecode:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = UniversalVehicleDecoderStepCheckDuration;
        }
        break;

    case UniversalVehicleDecoderStepCheckDuration:
        if(!level) {
            // Try to decode as OOK/ASK (2-level encoding)
            uint32_t avg_short = 0, avg_long = 0;
            uint8_t count_short = 0, count_long = 0;

            for(uint8_t i = 0; i < instance->pattern_index && i < 16; i++) {
                uint32_t dur = instance->pattern_analysis[i];
                if(dur < 500) {
                    avg_short += dur;
                    count_short++;
                } else if(dur < 2000) {
                    avg_long += dur;
                    count_long++;
                }
            }

            if(count_short > 0) avg_short /= count_short;
            if(count_long > 0) avg_long /= count_long;

            // Determine bit based on timing
            if(avg_short > 0 && avg_long > 0) {
                if(DURATION_DIFF(instance->decoder.te_last, avg_short) <
                   DURATION_DIFF(instance->decoder.te_last, avg_long)) {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                } else {
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                }

                instance->decoder.parser_step = UniversalVehicleDecoderStepDecode;

                // Check if we have enough bits and potential end
                if(instance->decoder.decode_count_bit >=
                   subghz_protocol_universal_vehicle_const.min_count_bit_for_found) {
                    if(duration > 5000) { // Potential end of transmission
                        instance->generic.data = instance->decoder.decode_data;
                        instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                        if(instance->base.callback)
                            instance->base.callback(&instance->base, instance->base.context);
                        instance->decoder.decode_data = 0;
                        instance->decoder.decode_count_bit = 0;
                        instance->pattern_index = 0;
                        instance->decoder.parser_step = UniversalVehicleDecoderStepReset;
                    }
                }
            }
        } else {
            instance->decoder.parser_step = UniversalVehicleDecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_universal_vehicle_parse_data(SubGhzBlockGeneric* instance) {
    // Attempt to parse data in common vehicle key fob formats
    // Format 1: Serial + Counter (common)
    if(instance->data_count_bit >= 24 && instance->data_count_bit <= 64) {
        // Try 32-bit serial format
        if(instance->data_count_bit >= 32) {
            instance->serial = (uint32_t)(instance->data & 0xFFFFFFFF);
            if(instance->data_count_bit >= 48) {
                instance->cnt = (uint16_t)((instance->data >> 32) & 0xFFFF);
            }
        } else {
            instance->serial = (uint32_t)(instance->data & 0xFFFFFF);
        }
    }
}

uint8_t subghz_protocol_decoder_universal_vehicle_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_universal_vehicle_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_universal_vehicle_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_universal_vehicle_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_universal_vehicle_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderUniversalVehicle* instance = context;

    subghz_protocol_universal_vehicle_parse_data(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Raw:%08lX%08lX\r\n"
        "Sn:%08lX Cnt:%04lX\r\n"
        "Protocol: Auto-detected",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt);
}
