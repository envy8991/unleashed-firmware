#pragma once

#include "base.h"

#define SUBGHZ_PROTOCOL_TOYOTA_KEY_NAME "Toyota Key"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzProtocolDecoderToyotaKey SubGhzProtocolDecoderToyotaKey;

extern const SubGhzProtocolDecoder subghz_protocol_toyota_key_decoder;
extern const SubGhzProtocol subghz_protocol_toyota_key;

void* subghz_protocol_decoder_toyota_key_alloc(SubGhzEnvironment* environment);
void subghz_protocol_decoder_toyota_key_free(void* context);
void subghz_protocol_decoder_toyota_key_reset(void* context);
void subghz_protocol_decoder_toyota_key_feed(void* context, bool level, uint32_t duration);
uint8_t subghz_protocol_decoder_toyota_key_get_hash_data(void* context);
SubGhzProtocolStatus subghz_protocol_decoder_toyota_key_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
SubGhzProtocolStatus subghz_protocol_decoder_toyota_key_deserialize(
    void* context,
    FlipperFormat* flipper_format);
void subghz_protocol_decoder_toyota_key_get_string(void* context, FuriString* output);

#ifdef __cplusplus
}
#endif
