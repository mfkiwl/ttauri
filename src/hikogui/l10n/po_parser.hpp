// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "translation.hpp"
#include "../i18n/module.hpp"
#include "../file/module.hpp"
#include "../parser/parser.hpp"
#include "../macros.hpp"
#include <string>
#include <vector>
#include <filesystem>

hi_export_module(hikogui.l10n.po_parser);

namespace hi::inline v1 {

struct po_translation {
    std::string msgctxt;
    std::string msgid;
    std::string msgid_plural;
    std::vector<std::string> msgstr;
};

struct po_translations {
    language_tag language;
    int nr_plural_forms;
    std::string plural_expression;
    std::vector<po_translation> translations;
};

namespace detail {

[[nodiscard]] inline parse_result<std::tuple<std::string, int, std::string>> parseLine(token_iterator token)
{
    std::string name;
    if ((*token == tokenizer_name_t::Name)) {
        name = static_cast<std::string>(*token++);
    } else {
        throw parse_error(std::format("{}: Expecting a name at start of each line", token->location));
    }

    int index = 0;
    if ((*token == tokenizer_name_t::Operator) && (*token == "[")) {
        token++;

        if ((*token == tokenizer_name_t::IntegerLiteral)) {
            index = static_cast<int>(*token++);
        } else {
            throw parse_error(std::format("{}: Expecting an integer literal as an index for {}", token->location, name));
        }

        if ((*token == tokenizer_name_t::Operator) && (*token == "]")) {
            token++;
        } else {
            throw parse_error(std::format("{}: The index on {} must terminate with a bracket ']'", token->location, name));
        }
    }

    std::string value;
    if ((*token == tokenizer_name_t::StringLiteral)) {
        value = static_cast<std::string>(*token++);
    } else {
        throw parse_error(std::format("{}: Expecting a value at end of each line", token->location));
    }

    while (true) {
        if ((*token == tokenizer_name_t::StringLiteral)) {
            value += static_cast<std::string>(*token++);
        } else {
            return {std::tuple{name, index, value}, token};
        }
    }
}

[[nodiscard]] inline parse_result<po_translation> parse_po_translation(token_iterator token)
{
    po_translation r;

    while (true) {
        if (ssize(r.msgstr) == 0) {
            if (auto result = parseLine(token)) {
                token = result.next_token;

                hilet[name, index, value] = *result;
                if (name == "msgctxt") {
                    r.msgctxt = value;

                } else if (name == "msgid") {
                    r.msgid = value;

                } else if (name == "msgid_plural") {
                    r.msgid_plural = value;

                } else if (name == "msgstr") {
                    while (ssize(r.msgstr) <= index) {
                        r.msgstr.push_back({});
                    }
                    r.msgstr[index] = value;

                } else {
                    throw parse_error(std::format("{}: Unexpected line {}", token->location, name));
                }

            } else {
                return {};
            }

        } else if ((*token == tokenizer_name_t::Name) && (*token == "msgstr")) {
            if (auto result = parseLine(token)) {
                token = result.next_token;
                hilet[name, index, value] = *result;

                while (ssize(r.msgstr) <= index) {
                    r.msgstr.push_back({});
                }
                r.msgstr[index] = value;

            } else {
                return {};
            }

        } else {
            return {r, token};
        }
    }
}

inline void parse_po_header(po_translations& r, std::string const& header)
{
    for (hilet& line : split(header, '\n')) {
        if (ssize(line) == 0) {
            // Skip empty header lines.
            continue;
        }

        auto split_line = split(line, ':');
        if (ssize(split_line) < 2) {
            throw parse_error(std::format("Unknown header '{}'", line));
        }

        hilet name = split_line.front();
        split_line.erase(split_line.begin());
        hilet value = join(split_line, ":");

        if (name == "Language") {
            r.language = language_tag{strip(value)};
        } else if (name == "Plural-Forms") {
            hilet plural_split = split(value, ';');
        }
    }
}

} // namespace detail

[[nodiscard]] inline po_translations parse_po(std::string_view text)
{
    po_translations r;

    auto tokens = parseTokens(text);
    hi_assert(tokens.back() == tokenizer_name_t::End);

    auto token = tokens.begin();
    while (*token != tokenizer_name_t::End) {
        if (auto result = detail::parse_po_translation(token)) {
            token = result.next_token;

            if (ssize(result.value.msgid) != 0) {
                r.translations.push_back(result.value);

            } else if (ssize(result.value.msgstr) == 1) {
                detail::parse_po_header(r, result.value.msgstr.front());

            } else {
                throw parse_error("Unknown .po header");
            }
        }
    }

    return r;
}

[[nodiscard]] inline po_translations parse_po(std::filesystem::path const& path)
{
    return parse_po(as_string_view(file_view{path}));
}

} // namespace hi::inline v1
