/// @file argparse.c
/// @brief Allocation-free command line parser for userspace tools.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <argparse.h>
#include <string.h>

static void ap_reset_event(ap_event_t *event)
{
    if (!event) {
        return;
    }
    event->kind       = AP_EVENT_POSITIONAL;
    event->option     = NULL;
    event->name       = NULL;
    event->value      = NULL;
    event->positional = NULL;
}

static void ap_reset_error(ap_error_t *error)
{
    if (!error) {
        return;
    }
    error->code        = AP_ERR_NONE;
    error->token       = NULL;
    error->option_name = NULL;
}

static void ap_set_error(ap_error_t *error, ap_error_code_t code, const char *token, const char *option_name)
{
    if (!error) {
        return;
    }
    error->code        = code;
    error->token       = token;
    error->option_name = option_name;
}

static const ap_option_t *ap_find_long_option_n(const ap_option_t *options, size_t option_count, const char *name,
                                                 size_t name_len)
{
    for (size_t i = 0; i < option_count; ++i) {
        const char *long_name = options[i].long_name;
        if (!long_name) {
            continue;
        }
        if ((strncmp(long_name, name, name_len) == 0) && (long_name[name_len] == '\0')) {
            return &options[i];
        }
    }
    return NULL;
}

static const ap_option_t *ap_find_short_option(const ap_option_t *options, size_t option_count, char short_name)
{
    for (size_t i = 0; i < option_count; ++i) {
        if (options[i].short_name == short_name) {
            return &options[i];
        }
    }
    return NULL;
}

void ap_parser_init(ap_parser_t *parser, int argc, char **argv)
{
    parser->argc         = argc;
    parser->argv         = argv;
    parser->index        = 1;
    parser->short_pos    = 0;
    parser->stop_options = false;
}

static ap_status_t ap_parse_long_option(ap_parser_t *parser, const ap_option_t *options, size_t option_count,
                                        ap_event_t *event, ap_error_t *error)
{
    const char *token  = parser->argv[parser->index];
    const char *name   = token + 2;
    const char *eq     = strchr(name, '=');
    size_t name_len    = eq ? (size_t)(eq - name) : strlen(name);
    const char *value  = eq ? (eq + 1) : NULL;
    const ap_option_t *opt;

    if (name_len == 0) {
        ap_set_error(error, AP_ERR_EMPTY_OPTION, token, token);
        parser->index++;
        return AP_STATUS_ERROR;
    }

    opt = ap_find_long_option_n(options, option_count, name, name_len);
    if (!opt) {
        ap_set_error(error, AP_ERR_UNKNOWN_OPTION, token, token);
        parser->index++;
        return AP_STATUS_ERROR;
    }

    if (opt->value_mode == AP_VALUE_NONE) {
        if (value != NULL) {
            ap_set_error(error, AP_ERR_UNEXPECTED_VALUE, token, token);
            parser->index++;
            return AP_STATUS_ERROR;
        }
    } else if (opt->value_mode == AP_VALUE_REQUIRED) {
        if (value == NULL) {
            if ((parser->index + 1) >= parser->argc) {
                ap_set_error(error, AP_ERR_MISSING_VALUE, token, token);
                parser->index++;
                return AP_STATUS_ERROR;
            }
            value = parser->argv[parser->index + 1];
            parser->index += 2;
        } else {
            parser->index++;
        }
    } else {
        parser->index++;
    }

    event->kind   = AP_EVENT_OPTION;
    event->option = opt;
    event->name   = token;
    event->value  = value;

    return AP_STATUS_OK;
}

static ap_status_t ap_parse_short_option(ap_parser_t *parser, const ap_option_t *options, size_t option_count,
                                         ap_event_t *event, ap_error_t *error)
{
    const char *token = parser->argv[parser->index];
    char short_name   = token[parser->short_pos];

    const ap_option_t *opt = ap_find_short_option(options, option_count, short_name);
    if (!opt) {
        ap_set_error(error, AP_ERR_UNKNOWN_OPTION, token, token);
        parser->index++;
        parser->short_pos = 0;
        return AP_STATUS_ERROR;
    }

    const char *value     = NULL;
    const char *remainder = &token[parser->short_pos + 1];

    if (opt->value_mode == AP_VALUE_NONE) {
        parser->short_pos++;
        if (token[parser->short_pos] == '\0') {
            parser->index++;
            parser->short_pos = 0;
        }
    } else if (opt->value_mode == AP_VALUE_REQUIRED) {
        if (*remainder != '\0') {
            value = remainder;
            parser->index++;
            parser->short_pos = 0;
        } else if ((parser->index + 1) < parser->argc) {
            value = parser->argv[parser->index + 1];
            parser->index += 2;
            parser->short_pos = 0;
        } else {
            ap_set_error(error, AP_ERR_MISSING_VALUE, token, token);
            parser->index++;
            parser->short_pos = 0;
            return AP_STATUS_ERROR;
        }
    } else {
        if (*remainder != '\0') {
            value = remainder;
        }
        parser->index++;
        parser->short_pos = 0;
    }

    event->kind   = AP_EVENT_OPTION;
    event->option = opt;
    event->name   = token;
    event->value  = value;
    return AP_STATUS_OK;
}

ap_status_t ap_parser_next(ap_parser_t *parser, const ap_option_t *options, size_t option_count, ap_event_t *event,
                           ap_error_t *error)
{
    ap_reset_event(event);
    ap_reset_error(error);

    if ((parser == NULL) || (options == NULL) || (event == NULL)) {
        ap_set_error(error, AP_ERR_INVALID_ARGUMENT, NULL, NULL);
        return AP_STATUS_ERROR;
    }

    while (parser->index < parser->argc) {
        const char *token = parser->argv[parser->index];

        if (parser->stop_options) {
            event->kind       = AP_EVENT_POSITIONAL;
            event->positional = token;
            parser->index++;
            return AP_STATUS_OK;
        }

        if (parser->short_pos > 0) {
            return ap_parse_short_option(parser, options, option_count, event, error);
        }

        if ((strcmp(token, "--") == 0)) {
            parser->stop_options = true;
            parser->index++;
            continue;
        }

        if ((token[0] != '-') || (token[1] == '\0')) {
            event->kind       = AP_EVENT_POSITIONAL;
            event->positional = token;
            parser->index++;
            return AP_STATUS_OK;
        }

        if (token[1] == '-') {
            return ap_parse_long_option(parser, options, option_count, event, error);
        }

        parser->short_pos = 1;
        return ap_parse_short_option(parser, options, option_count, event, error);
    }

    return AP_STATUS_END;
}

const char *ap_error_code_to_string(ap_error_code_t code)
{
    switch (code) {
    case AP_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case AP_ERR_UNKNOWN_OPTION:
        return "unknown option";
    case AP_ERR_MISSING_VALUE:
        return "missing option value";
    case AP_ERR_UNEXPECTED_VALUE:
        return "unexpected option value";
    case AP_ERR_EMPTY_OPTION:
        return "empty option";
    case AP_ERR_NONE:
    default:
        return "no error";
    }
}
