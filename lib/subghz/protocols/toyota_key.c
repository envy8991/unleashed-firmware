#include "toyota_key.h"

#include "../blocks/const.h"
#include "../blocks/decoder.h"
#include "../blocks/generic.h"
#include "../blocks/math.h"

#define TAG "SubGhzProtocolToyotaKey"

static const SubGhzBlockConst subghz_protocol_toyota_key_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 40,
};

struct SubGhzProtocolDecoderToyotaKey {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

typedef enum {
    ToyotaKeyDecoderStepReset = 0,
    ToyotaKeyDecoderStepCheckPreambula,
    ToyotaKeyDecoderStepSaveDuration,
    ToyotaKeyDecoderStepCheckDuration,
} ToyotaKeyDecoderStep;

const SubGhzProtocolDecoder subghz_protocol_toyota_key_decoder = {
    .alloc = subghz_protocol_decoder_toyota_key_alloc,
    .free = subghz_protocol_decoder_toyota_key_free,
    .feed = subghz_protocol_decoder_toyota_key_feed,
    .reset = subghz_protocol_decoder_toyota_key_reset,
    .get_hash_data = subghz_protocol_decoder_toyota_key_get_hash_data,
    .serialize = subghz_protocol_decoder_toyota_key_serialize,
    .deserialize = subghz_protocol_decoder_toyota_key_deserialize,
    .get_string = subghz_protocol_decoder_toyota_key_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_toyota_key_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol subghz_protocol_toyota_key = {
    .name = SUBGHZ_PROTOCOL_TOYOTA_KEY_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Cars,
    .decoder = &subghz_protocol_toyota_key_decoder,
    .encoder = &subghz_protocol_toyota_key_encoder,
};

void* subghz_protocol_decoder_toyota_key_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderToyotaKey* instance = malloc(sizeof(SubGhzProtocolDecoderToyotaKey));
    instance->base.protocol = &subghz_protocol_toyota_key;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_toyota_key_free(void* context) {
    furi_assert(context);
    free(context);
}

void subghz_protocol_decoder_toyota_key_reset(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;
    instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
}

void subghz_protocol_decoder_toyota_key_feed(void* context, bool level, uint32_t duration) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;

    switch(instance->decoder.parser_step) {
    case ToyotaKeyDecoderStepReset:
        if((level) && (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_short) <
                      subghz_protocol_toyota_key_const.te_delta)) {
            instance->decoder.parser_step = ToyotaKeyDecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 1;
        }
        break;
    case ToyotaKeyDecoderStepCheckPreambula:
        if(level) {
            if((DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_short) <
                subghz_protocol_toyota_key_const.te_delta) ||
               (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_long) <
                subghz_protocol_toyota_key_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
            }
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_short) <
             subghz_protocol_toyota_key_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_toyota_key_const.te_short) <
             subghz_protocol_toyota_key_const.te_delta)) {
            instance->header_count++;
            break;
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_long) <
             subghz_protocol_toyota_key_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_toyota_key_const.te_long) <
             subghz_protocol_toyota_key_const.te_delta)) {
            if(instance->header_count >= 10) {
                instance->decoder.parser_step = ToyotaKeyDecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
            } else {
                instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
        }
        break;
    case ToyotaKeyDecoderStepSaveDuration:
        if(level) {
            if(duration >=
               (subghz_protocol_toyota_key_const.te_long + subghz_protocol_toyota_key_const.te_delta * 3)) {
                instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
                if(instance->decoder.decode_count_bit >=
                   subghz_protocol_toyota_key_const.min_count_bit_for_found) {
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
                instance->decoder.parser_step = ToyotaKeyDecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
        }
        break;
    case ToyotaKeyDecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_toyota_key_const.te_short) <
                subghz_protocol_toyota_key_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_short) <
                subghz_protocol_toyota_key_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = ToyotaKeyDecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_toyota_key_const.te_long) <
                 subghz_protocol_toyota_key_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_toyota_key_const.te_long) <
                 subghz_protocol_toyota_key_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = ToyotaKeyDecoderStepSaveDuration;
            } else {
                instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = ToyotaKeyDecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_toyota_key_parse_data(SubGhzBlockGeneric* instance) {
    // Toyota key fob format: typically 40-64 bits
    // Format varies by model year
    if(instance->data_count_bit >= 40) {
        instance->serial = (uint32_t)(instance->data & 0xFFFFFFFF);
        if(instance->data_count_bit >= 56) {
            instance->cnt = (uint16_t)((instance->data >> 32) & 0xFFFF);
        }
    }
}

uint8_t subghz_protocol_decoder_toyota_key_get_hash_data(void* context) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_toyota_key_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus subghz_protocol_decoder_toyota_key_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_toyota_key_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_toyota_key_get_string(void* context, FuriString* output) {
    furi_assert(context);
    SubGhzProtocolDecoderToyotaKey* instance = context;

    subghz_protocol_toyota_key_parse_data(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%08lX Cnt:%04lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt);
}
