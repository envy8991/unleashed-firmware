#pragma once

#include "base.h"

#define SUBGHZ_PROTOCOL_HITAG2_NAME "HiTag2"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SubGhzProtocolDecoderHiTag2 SubGhzProtocolDecoderHiTag2;
typedef struct SubGhzProtocolEncoderHiTag2 SubGhzProtocolEncoderHiTag2;

extern const SubGhzProtocolDecoder subghz_protocol_hitag2_decoder;
extern const SubGhzProtocolEncoder subghz_protocol_hitag2_encoder;
extern const SubGhzProtocol subghz_protocol_hitag2;

/**
 * Allocate SubGhzProtocolDecoderHiTag2.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return SubGhzProtocolDecoderHiTag2* pointer to a SubGhzProtocolDecoderHiTag2 instance
 */
void* subghz_protocol_decoder_hitag2_alloc(SubGhzEnvironment* environment);

/**
 * Free SubGhzProtocolDecoderHiTag2.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 */
void subghz_protocol_decoder_hitag2_free(void* context);

/**
 * Reset decoder SubGhzProtocolDecoderHiTag2.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 */
void subghz_protocol_decoder_hitag2_reset(void* context);

/**
 * Feed a SubGhzProtocolDecoderHiTag2.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void subghz_protocol_decoder_hitag2_feed(void* context, bool level, uint32_t duration);

/**
 * Getting a hash data of input.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 * @return hash Hash sum
 */
uint8_t subghz_protocol_decoder_hitag2_get_hash_data(void* context);

/**
 * Serialize data SubGhzProtocolDecoderHiTag2.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus subghz_protocol_decoder_hitag2_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data SubGhzProtocolDecoderHiTag2.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus subghz_protocol_decoder_hitag2_deserialize(
    void* context,
    FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a SubGhzProtocolDecoderHiTag2 instance
 * @param output Resulting text
 */
void subghz_protocol_decoder_hitag2_get_string(void* context, FuriString* output);

/**
 * Allocate SubGhzProtocolEncoderHiTag2.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return Pointer to a SubGhzProtocolEncoderHiTag2 instance
 */
void* subghz_protocol_encoder_hitag2_alloc(SubGhzEnvironment* environment);

/**
 * Free SubGhzProtocolEncoderHiTag2.
 * @param context Pointer to a SubGhzProtocolEncoderHiTag2 instance
 */
void subghz_protocol_encoder_hitag2_free(void* context);

/**
 * Deserialize and generating an upload to send.
 * @param context Pointer to a SubGhzProtocolEncoderHiTag2 instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus
    subghz_protocol_encoder_hitag2_deserialize(void* context, FlipperFormat* flipper_format);

/**
 * Forced transmission stop.
 * @param context Pointer to a SubGhzProtocolEncoderHiTag2 instance
 */
void subghz_protocol_encoder_hitag2_stop(void* context);

/**
 * Getting the level and duration of the upload to be loaded into DMA.
 * @param context Pointer to a SubGhzProtocolEncoderHiTag2 instance
 * @return LevelDuration
 */
LevelDuration subghz_protocol_encoder_hitag2_yield(void* context);

#ifdef __cplusplus
}
#endif
