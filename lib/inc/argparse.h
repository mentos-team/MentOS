/// @file argparse.h
/// @brief Allocation-free command line parser for userspace tools.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef enum ap_value_mode {
    AP_VALUE_NONE = 0,
    AP_VALUE_REQUIRED,
    AP_VALUE_OPTIONAL,
} ap_value_mode_t;

typedef struct ap_option {
    char short_name;
    const char *long_name;
    ap_value_mode_t value_mode;
    const char *value_name;
    const char *help;
} ap_option_t;

typedef enum ap_error_code {
    AP_ERR_NONE = 0,
    AP_ERR_INVALID_ARGUMENT,
    AP_ERR_UNKNOWN_OPTION,
    AP_ERR_MISSING_VALUE,
    AP_ERR_UNEXPECTED_VALUE,
    AP_ERR_EMPTY_OPTION,
} ap_error_code_t;

typedef struct ap_error {
    ap_error_code_t code;
    const char *token;
    const char *option_name;
} ap_error_t;

typedef enum ap_event_kind {
    AP_EVENT_OPTION = 0,
    AP_EVENT_POSITIONAL,
} ap_event_kind_t;

typedef struct ap_event {
    ap_event_kind_t kind;
    const ap_option_t *option;
    const char *name;
    const char *value;
    const char *positional;
} ap_event_t;

typedef enum ap_status {
    AP_STATUS_OK = 0,
    AP_STATUS_END,
    AP_STATUS_ERROR,
} ap_status_t;

typedef struct ap_parser {
    int argc;
    char **argv;
    int index;
    int short_pos;
    bool_t stop_options;
} ap_parser_t;

void ap_parser_init(ap_parser_t *parser, int argc, char **argv);

ap_status_t ap_parser_next(ap_parser_t *parser, const ap_option_t *options, size_t option_count, ap_event_t *event,
                           ap_error_t *error);

const char *ap_error_code_to_string(ap_error_code_t code);
