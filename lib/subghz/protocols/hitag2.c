#include "hitag2.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/encoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "SubGhzProtocolHiTag2"

static const SubGhzBlockConst subghz_protocol_hitag2_const = {
    .te_short = 256,
    .te_long = 512,
    .te_delta = 100,
    .min_count_bit_for_found = 50,
};

struct SubGhzProtocolDecoderHiTag2 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t header_count;
};

struct SubGhzProtocolEncoderHiTag2 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    HiTag2DecoderStepReset = 0,
    HiTag2DecoderStepCheckPreambula,
    HiTag2DecoderStepSaveDuration,
    HiTag2DecoderStepCheckDuration,
} HiTag2DecoderStep;

const SubGhzProtocolDecoder subghz_protocol_hitag2_decoder = {
    .alloc = subghz_protocol_decoder_hitag2_alloc,
    .free = subghz_protocol_decoder_hitag2_free,

    .feed = subghz_protocol_decoder_hitag2_feed,
    .reset = subghz_protocol_decoder_hitag2_reset,

    .get_hash_data = subghz_protocol_decoder_hitag2_get_hash_data,
    .serialize = subghz_protocol_decoder_hitag2_serialize,
    .deserialize = subghz_protocol_decoder_hitag2_deserialize,
    .get_string = subghz_protocol_decoder_hitag2_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_hitag2_encoder = {
    .alloc = NULL,
    .free = NULL,

    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol subghz_protocol_hitag2 = {
    .name = SUBGHZ_PROTOCOL_HITAG2_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Cars,

    .decoder = &subghz_protocol_hitag2_decoder,
    .encoder = &subghz_protocol_hitag2_encoder,
};

void* subghz_protocol_decoder_hitag2_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderHiTag2* instance = malloc(sizeof(SubGhzProtocolDecoderHiTag2));
    instance->base.protocol = &subghz_protocol_hitag2;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_hitag2_free(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;
    free(instance);
}

void subghz_protocol_decoder_hitag2_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;
    instance->decoder.parser_step = HiTag2DecoderStepReset;
}

void subghz_protocol_decoder_hitag2_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;

    switch(instance->decoder.parser_step) {
    case HiTag2DecoderStepReset:
        if((level) && (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_short) <
                      subghz_protocol_hitag2_const.te_delta)) {
            instance->decoder.parser_step = HiTag2DecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 1;
        }
        break;
    case HiTag2DecoderStepCheckPreambula:
        if(level) {
            if((DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_short) <
                subghz_protocol_hitag2_const.te_delta) ||
               (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_long) <
                subghz_protocol_hitag2_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = HiTag2DecoderStepReset;
            }
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_short) <
             subghz_protocol_hitag2_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_hitag2_const.te_short) <
             subghz_protocol_hitag2_const.te_delta)) {
            instance->header_count++;
            break;
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_long) <
             subghz_protocol_hitag2_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_hitag2_const.te_long) <
             subghz_protocol_hitag2_const.te_delta)) {
            if(instance->header_count >= 8) {
                instance->decoder.parser_step = HiTag2DecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            } else {
                instance->decoder.parser_step = HiTag2DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = HiTag2DecoderStepReset;
        }
        break;
    case HiTag2DecoderStepSaveDuration:
        if(level) {
            if(duration >=
               (subghz_protocol_hitag2_const.te_long + subghz_protocol_hitag2_const.te_delta * 3)) {
                instance->decoder.parser_step = HiTag2DecoderStepReset;
                if(instance->decoder.decode_count_bit >=
                   subghz_protocol_hitag2_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->header_count = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = HiTag2DecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = HiTag2DecoderStepReset;
        }
        break;
    case HiTag2DecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_hitag2_const.te_short) <
                subghz_protocol_hitag2_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_short) <
                subghz_protocol_hitag2_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = HiTag2DecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_hitag2_const.te_long) <
                 subghz_protocol_hitag2_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_hitag2_const.te_long) <
                 subghz_protocol_hitag2_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = HiTag2DecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = HiTag2DecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = HiTag2DecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_hitag2_check_remote_controller(SubGhzBlockGeneric* instance) {
    // HiTag2 typically has 48-bit or 64-bit data
    // Format: UID (32-40 bits) + Configuration/Encryption data
    if(instance->data_count_bit >= 48) {
        instance->serial = (uint32_t)(instance->data & 0xFFFFFFFF);
        instance->cnt = (uint16_t)((instance->data >> 32) & 0xFFFF);
    } else {
        instance->serial = (uint32_t)(instance->data & 0xFFFFFF);
        instance->cnt = 0;
    }
}

uint8_t subghz_protocol_decoder_hitag2_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_hitag2_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    subghz_protocol_decoder_hitag2_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_hitag2_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_hitag2_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderHiTag2* instance = context;

    subghz_protocol_hitag2_check_remote_controller(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "UID:%08lX Cnt:%04lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt);
}

// Encoder stubs (HiTag2 encoding requires encryption, typically read-only)
void* subghz_protocol_encoder_hitag2_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    return NULL;
}

void subghz_protocol_encoder_hitag2_free(void* context) {
    UNUSED(context);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_hitag2_deserialize(void* context, FlipperFormat* flipper_format) {
    UNUSED(context);
    UNUSED(flipper_format);
    return SubGhzProtocolStatusError;
}

void subghz_protocol_encoder_hitag2_stop(void* context) {
    UNUSED(context);
}

LevelDuration subghz_protocol_encoder_hitag2_yield(void* context) {
    UNUSED(context);
    return level_duration_reset();
}
