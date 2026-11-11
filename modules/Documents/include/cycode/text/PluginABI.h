#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CYCODE_READER_PLUGIN_ABI 1u
/* UTF-8 JSON only across the ABI. The plugin owns the returned string until
   the next call. Commands execute serially and must not call GUI functions. */
typedef struct CyCodeReaderPluginV1 {
    uint32_t abi;
    uint32_t size;
    const char *(*execute)(const char *command, const char *input_json);
} CyCodeReaderPluginV1;
typedef const CyCodeReaderPluginV1 *(*CyCodeReaderPluginEntry)(void);
#ifdef __cplusplus
}
#endif
