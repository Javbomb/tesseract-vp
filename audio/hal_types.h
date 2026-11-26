/* Android 9 audio HAL ABI subset (32-bit). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#define MAKE_TAG_CONSTANT(A, B, C, D) (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))
#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
#define HAL_MODULE_INFO_SYM HMI

typedef uint32_t audio_format_t;
typedef uint32_t audio_channel_mask_t;
typedef uint32_t audio_devices_t;
typedef uint32_t audio_output_flags_t;
typedef int audio_io_handle_t;

#define AUDIO_FORMAT_PCM_16_BIT    0x00000001u
#define AUDIO_FORMAT_AC3           0x09000000u
#define AUDIO_FORMAT_E_AC3         0x0A000000u
#define AUDIO_FORMAT_E_AC3_JOC     0x0A000001u
#define AUDIO_FORMAT_DTS           0x0B000000u
#define AUDIO_FORMAT_DTS_HD        0x0C000000u
#define AUDIO_FORMAT_IEC61937      0x0D000000u
#define AUDIO_FORMAT_DOLBY_TRUEHD  0x0E000000u
#define AUDIO_FORMAT_MAIN_MASK     0xFF000000u

#define AUDIO_CHANNEL_OUT_7POINT1  0x63Fu
#define AUDIO_OUTPUT_FLAG_DIRECT      0x1u
#define AUDIO_OUTPUT_FLAG_HW_AV_SYNC  0x40u
#define AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD 0x10u
#define AUDIO_OUTPUT_FLAG_MMAP_NOIRQ  0x4000u

struct hw_module_t;
struct hw_device_t;

struct hw_module_methods_t {
    int (*open)(const struct hw_module_t *module, const char *id, struct hw_device_t **device);
};

struct hw_module_t {
    uint32_t tag;
    uint16_t module_api_version;
    uint16_t hal_api_version;
    const char *id;
    const char *name;
    const char *author;
    struct hw_module_methods_t *methods;
    void *dso;
    uint32_t reserved[32 - 7];
};

struct hw_device_t {
    uint32_t tag;
    uint32_t version;
    struct hw_module_t *module;
    uint32_t reserved[12];
    int (*close)(struct hw_device_t *device);
};

struct audio_offload_info_t {
    uint16_t version;
    uint16_t size;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    uint32_t stream_type;
    uint32_t bit_rate;
    int64_t duration_us;
    bool has_video;
    bool is_streaming;
    uint32_t bit_width;
    uint32_t offload_buffer_size;
    uint32_t usage;
};

struct audio_config {
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    struct audio_offload_info_t offload_info;
    uint32_t frame_count;
};

struct audio_stream {
    uint32_t (*get_sample_rate)(const struct audio_stream *stream);
    int (*set_sample_rate)(struct audio_stream *stream, uint32_t rate);
    size_t (*get_buffer_size)(const struct audio_stream *stream);
    audio_channel_mask_t (*get_channels)(const struct audio_stream *stream);
    audio_format_t (*get_format)(const struct audio_stream *stream);
    int (*set_format)(struct audio_stream *stream, audio_format_t format);
    int (*standby)(struct audio_stream *stream);
    int (*dump)(const struct audio_stream *stream, int fd);
    audio_devices_t (*get_device)(const struct audio_stream *stream);
    int (*set_device)(struct audio_stream *stream, audio_devices_t device);
    int (*set_parameters)(struct audio_stream *stream, const char *kv_pairs);
    char *(*get_parameters)(const struct audio_stream *stream, const char *keys);
    int (*add_audio_effect)(const struct audio_stream *stream, void *effect);
    int (*remove_audio_effect)(const struct audio_stream *stream, void *effect);
};

struct audio_stream_out {
    struct audio_stream common;
    uint32_t (*get_latency)(const struct audio_stream_out *stream);
    int (*set_volume)(struct audio_stream_out *stream, float left, float right);
    ssize_t (*write)(struct audio_stream_out *stream, const void *buffer, size_t bytes);
    int (*get_render_position)(const struct audio_stream_out *stream, uint32_t *dsp_frames);
    int (*get_next_write_timestamp)(const struct audio_stream_out *stream, int64_t *timestamp);
    int (*set_callback)(struct audio_stream_out *stream, void *callback, void *cookie);
    int (*pause)(struct audio_stream_out *stream);
    int (*resume)(struct audio_stream_out *stream);
    int (*drain)(struct audio_stream_out *stream, int type);
    int (*flush)(struct audio_stream_out *stream);
    int (*get_presentation_position)(const struct audio_stream_out *stream, uint64_t *frames,
                                     struct timespec *timestamp);
};

struct audio_hw_device {
    struct hw_device_t common;
    uint32_t (*get_supported_devices)(const struct audio_hw_device *dev);
    int (*init_check)(const struct audio_hw_device *dev);
    int (*set_voice_volume)(struct audio_hw_device *dev, float volume);
    int (*set_master_volume)(struct audio_hw_device *dev, float volume);
    int (*get_master_volume)(struct audio_hw_device *dev, float *volume);
    int (*set_mode)(struct audio_hw_device *dev, int mode);
    int (*set_mic_mute)(struct audio_hw_device *dev, bool state);
    int (*get_mic_mute)(const struct audio_hw_device *dev, bool *state);
    int (*set_parameters)(struct audio_hw_device *dev, const char *kv_pairs);
    char *(*get_parameters)(const struct audio_hw_device *dev, const char *keys);
    size_t (*get_input_buffer_size)(const struct audio_hw_device *dev, const struct audio_config *config);
    int (*open_output_stream)(struct audio_hw_device *dev, audio_io_handle_t handle,
                              audio_devices_t devices, audio_output_flags_t flags,
                              struct audio_config *config, struct audio_stream_out **stream_out,
                              const char *address);
    void (*close_output_stream)(struct audio_hw_device *dev, struct audio_stream_out *stream_out);
};
