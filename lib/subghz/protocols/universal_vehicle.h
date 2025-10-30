#pragma once

#include "base.h"

#define SUBGHZ_PROTOCOL_UNIVERSAL_VEHICLE_NAME "Universal Vehicle"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Universal Vehicle Key Fob Decoder
 * Attempts to decode unknown vehicle key fob signals using pattern recognition
 * and multiple protocol templates.
 */

typedef struct SubGhzProtocolDecoderUniversalVehicle SubGhzProtocolDecoderUniversalVehicle;

extern const SubGhzProtocolDecoder subghz_protocol_universal_vehicle_decoder;
extern const SubGhzProtocol subghz_protocol_universal_vehicle;

/**
 * Allocate SubGhzProtocolDecoderUniversalVehicle.
 * @param environment Pointer to a SubGhzEnvironment instance
 * @return SubGhzProtocolDecoderUniversalVehicle* pointer to instance
 */
void* subghz_protocol_decoder_universal_vehicle_alloc(SubGhzEnvironment* environment);

/**
 * Free SubGhzProtocolDecoderUniversalVehicle.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 */
void subghz_protocol_decoder_universal_vehicle_free(void* context);

/**
 * Reset decoder SubGhzProtocolDecoderUniversalVehicle.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 */
void subghz_protocol_decoder_universal_vehicle_reset(void* context);

/**
 * Feed a SubGhzProtocolDecoderUniversalVehicle.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 * @param level Signal level true-high false-low
 * @param duration Duration of this level in, us
 */
void subghz_protocol_decoder_universal_vehicle_feed(
    void* context,
    bool level,
    uint32_t duration);

/**
 * Getting a hash data of input.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 * @return hash Hash sum
 */
uint8_t subghz_protocol_decoder_universal_vehicle_get_hash_data(void* context);

/**
 * Serialize data SubGhzProtocolDecoderUniversalVehicle.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @param preset The modulation on which the signal was received, SubGhzRadioPreset
 * @return status
 */
SubGhzProtocolStatus subghz_protocol_decoder_universal_vehicle_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);

/**
 * Deserialize data SubGhzProtocolDecoderUniversalVehicle.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 * @param flipper_format Pointer to a FlipperFormat instance
 * @return status
 */
SubGhzProtocolStatus subghz_protocol_decoder_universal_vehicle_deserialize(
    void* context,
    FlipperFormat* flipper_format);

/**
 * Getting a textual representation of the received data.
 * @param context Pointer to a SubGhzProtocolDecoderUniversalVehicle instance
 * @param output Resulting text
 */
void subghz_protocol_decoder_universal_vehicle_get_string(void* context, FuriString* output);

#ifdef __cplusplus
}
#endif
