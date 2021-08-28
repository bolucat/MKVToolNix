/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   BCP 47 language tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <fmt/ranges.h>

#include "common/bcp47.h"
#include "common/bcp47_re.h"
#include "common/iana_language_subtag_registry.h"
#include "common/iso639.h"
#include "common/iso3166.h"
#include "common/iso15924.h"
#include "common/list_utils.h"
#include "common/qt.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"

namespace mtx::bcp47 {

bool
operator <(language_c::extension_t const &a,
           language_c::extension_t const &b) {
  return mtx::string::to_lower_ascii(a.identifier) < mtx::string::to_lower_ascii(b.identifier);
}

bool language_c::ms_disabled = false;

language_c::extension_t::extension_t(std::string const &identifier_,
                                     std::vector<std::string> const &extensions_)
  : identifier{identifier_}
  , extensions{extensions_}
{
}

std::string
language_c::extension_t::format()
  const noexcept {
  if (identifier.empty() || extensions.empty())
    return {};
  return fmt::format("{0}-{1}", identifier, mtx::string::join(extensions, "-"));
}

bool
language_c::extension_t::operator ==(extension_t const &other)
  const noexcept {
  if (identifier != other.identifier)
    return false;

  return extensions == extensions;
}

bool
language_c::extension_t::operator !=(extension_t const &other)
  const noexcept {
  return !(*this == other);
}

// ------------------------------------------------------------

void
language_c::clear()
  noexcept {
  *this = mtx::bcp47::language_c{};
}

bool
language_c::is_valid()
  const noexcept {
  return m_valid;
}

bool
language_c::has_valid_iso639_code()
  const noexcept {
  return m_valid && !m_language.empty();
}

bool
language_c::has_valid_iso639_2_code()
  const noexcept {
  if (!m_valid || m_language.empty())
    return false;

  auto language_opt = mtx::iso639::look_up(get_language());
  return language_opt && language_opt->is_part_of_iso639_2;
}

std::string const &
language_c::get_error()
  const noexcept {
  return m_parser_error;
}

std::string
language_c::dump()
  const noexcept{
  return fmt::format("[valid {0} language {1} extended_language_subtags {2} script {3} region {4} variants {5} extensions {6} private_use {7} parser_error {8}]",
                     m_valid, m_language, m_extended_language_subtags, m_script, m_region, m_variants, m_extensions, m_private_use, m_parser_error);
}

std::string
language_c::format(bool force)
  const noexcept{
  if (force)
    return format_internal(true);

  if (!m_formatted_up_to_date) {
    m_formatted            = format_internal(force);
    m_formatted_up_to_date = true;
  }

  return m_formatted;
}

std::string
language_c::format_internal(bool force)
  const noexcept {
  if (!m_valid && !force)
    return {};

  auto output = mtx::string::to_lower_ascii(m_language);

  for (auto const &subtag : m_extended_language_subtags)
    output += fmt::format("-{}", mtx::string::to_lower_ascii(subtag));

  if (!m_script.empty())
    output += fmt::format("-{}{}", mtx::string::to_upper_ascii(m_script.substr(0, 1)), mtx::string::to_lower_ascii(m_script.substr(1)));

  if (!m_region.empty())
    output += fmt::format("-{}", mtx::string::to_upper_ascii(m_region));

  for (auto const &variant : m_variants)
    output += fmt::format("-{}", mtx::string::to_lower_ascii(variant));

  for (auto const &extension : m_extensions)
    output += fmt::format("-{}", mtx::string::to_lower_ascii(extension.format()));

  if (!m_private_use.empty()) {
    if (!output.empty())
      output += "-";

    output += "x";

    for (auto const &private_use : m_private_use)
      output += fmt::format("-{}", mtx::string::to_lower_ascii(private_use));
  }

  return output;
}

std::string
language_c::format_long(bool force)
  const noexcept {
  auto formatted = format(force);

  if (formatted.empty())
    return formatted;

  std::string text;

  if (!get_language().empty()) {
    auto language_opt = mtx::iso639::look_up(get_language());
    if (language_opt)
      return fmt::format("{0} ({1})", language_opt->english_name, formatted);
  }

  return formatted;
}

bool
language_c::parse_language(std::string const &code) {
  auto language = mtx::iso639::look_up(code);
  if (!language) {
    m_parser_error = fmt::format(Y("The value '{}' is not a valid ISO 639 language code."), code);
    return false;
  }

  m_language = !language->alpha_2_code.empty() ? language->alpha_2_code : language->alpha_3_code;

  return true;
}

bool
language_c::parse_script(std::string const &code) {
  auto script = mtx::iso15924::look_up(code);
  if (!script) {
    m_parser_error = fmt::format(Y("The value '{}' is not a valid ISO 15924 script code."), code);
    return false;
  }

  m_script = script->code;

  return true;
}

bool
language_c::parse_region(std::string const &code) {
  if (code.length() == 2) {
    auto region = mtx::iso3166::look_up(code);
    if (!region) {
      m_parser_error = fmt::format(Y("The value '{}' is not a valid ISO 3166-1 country code."), code);
      return false;
    }

    m_region = region->alpha_2_code;

    return true;
  }

  auto normalized_code = to_utf8(Q(code).replace(QRegularExpression{"^0+"}, {}));
  if (normalized_code.empty())
    normalized_code = "0";

  auto number = 0u;
  mtx::string::parse_number(normalized_code, number);

  auto region = mtx::iso3166::look_up(number);
  if (!region) {
    m_parser_error = fmt::format(Y("The value '{}' is not a valid UN M.49 country number code."), code);
    return false;
  }

  if (region->alpha_2_code.empty())
    m_region = fmt::format("{0:03}", region->number);
  else
    m_region = region->alpha_2_code;

  return true;
}

bool
language_c::parse_extlangs_or_variants(std::string const &str,
                                       bool is_extlangs) {
  auto const current_str = mtx::string::to_lower_ascii(format_internal(true));

  for (auto const &code : mtx::string::split(str.substr(1), "-")) {
    auto entry = is_extlangs ? mtx::iana::language_subtag_registry::look_up_extlang(code)
               :               mtx::iana::language_subtag_registry::look_up_variant(code);

    if (!entry) {
      auto message   = is_extlangs ? Y("The value '{}' is not part of the IANA Language Subtag Registry for extended language subtags.")
                     :               Y("The value '{}' is not part of the IANA Language Subtag Registry for language variants.");
      m_parser_error = fmt::format(message, code);
      return false;
    }

    if (is_extlangs)
      m_extended_language_subtags.push_back(entry->code);
    else
      m_variants.push_back(entry->code);
  }

  return true;
}

bool
language_c::parse_extensions(std::string const &str) {
  if (str.empty())
    return true;

  for (auto &part : mtx::string::split(mtx::string::to_lower_ascii(str.substr(1)), "-"s))
    if (part.size() == 1)
      m_extensions.emplace_back(part, std::vector<std::string>{});

    else
      m_extensions.back().extensions.emplace_back(part);

  std::sort(m_extensions.begin(), m_extensions.end());

  return validate_extensions();
}

bool
language_c::matches_prefix(language_c const &prefix,
                           std::size_t extlang_or_variant_index,
                           bool is_extlang,
                           prefix_restrictions_t const &restrictions)
  const noexcept {
  if (   ( is_extlang && !m_extended_language_subtags.empty() && (extlang_or_variant_index > (prefix.m_extended_language_subtags.size())))
      || (!is_extlang && !m_variants                 .empty() && (extlang_or_variant_index > (prefix.m_variants                 .size()))))
    return false;

  if (   (restrictions.language                  && prefix.m_language                 .empty() && !m_language                 .empty())
      || (restrictions.extended_language_subtags && prefix.m_extended_language_subtags.empty() && !m_extended_language_subtags.empty())
      || (restrictions.script                    && prefix.m_script                   .empty() && !m_script                   .empty())
      || (restrictions.region                    && prefix.m_region                   .empty() && !m_region                   .empty())
      || (restrictions.variants                  && prefix.m_variants                 .empty() && !m_variants                 .empty()))
    return false;

  std::vector<std::string> this_relevant_parts;

  if (!prefix.m_language.empty())
    this_relevant_parts.emplace_back(m_language);

  for (auto const &extlang : m_extended_language_subtags)
    this_relevant_parts.emplace_back(extlang);

  if (!prefix.m_script.empty())
    this_relevant_parts.emplace_back(m_script);

  if (!prefix.m_region.empty())
    this_relevant_parts.emplace_back(m_region);

  for (auto const &variant : m_variants)
    this_relevant_parts.emplace_back(variant);

  auto this_relevant_formatted = mtx::string::join(this_relevant_parts, "-");
  auto prefix_formatted        = prefix.format() + "-";

  if (this_relevant_formatted.size() < prefix_formatted.size())
    return false;

  this_relevant_formatted.resize(prefix_formatted.size());

  return balg::iequals(prefix_formatted, this_relevant_formatted);
}

bool
language_c::validate_one_extlang_or_variant(std::size_t extlang_or_variant_index,
                                            bool is_extlang) {
  auto const &extlang_or_variant_code = is_extlang ? m_extended_language_subtags[extlang_or_variant_index]
                                      :              m_variants[extlang_or_variant_index];
  auto extlang_or_variant             = is_extlang ? mtx::iana::language_subtag_registry::look_up_extlang(extlang_or_variant_code)
                                      :              mtx::iana::language_subtag_registry::look_up_variant(extlang_or_variant_code);

  if (!extlang_or_variant)                   // Should not happen as the parsing checks this already.
    return false;

  if (extlang_or_variant->prefixes.empty())
    return true;

  prefix_restrictions_t restrictions;
  std::vector<language_c> parsed_prefixes;

  auto account = [](bool &value, bool is_unset) {
    if (!value && !is_unset)
      value = true;
  };

  for (auto const &prefix : extlang_or_variant->prefixes) {
    parsed_prefixes.emplace_back(parse(prefix));
    auto const &tag = parsed_prefixes.back();

    account(restrictions.language,                  tag.m_language.empty());
    account(restrictions.extended_language_subtags, tag.m_extended_language_subtags.empty());
    account(restrictions.script,                    tag.m_script.empty());
    account(restrictions.region,                    tag.m_region.empty());
    account(restrictions.variants,                  tag.m_variants.empty());
  }

  for (auto const &parsed_prefix : parsed_prefixes)
    if (matches_prefix(parsed_prefix, extlang_or_variant_index, is_extlang, restrictions))
      return true;

  auto message   = is_extlang ? Y("The extended language subtag '{}' must only be used with one of the following prefixes: {}.")
                 :              Y("The variant '{}' must only be used with one of the following prefixes: {}.");
  m_parser_error = fmt::format(message, extlang_or_variant_code, fmt::join(extlang_or_variant->prefixes, ", "));

  return false;
}

bool
language_c::validate_extlangs_or_variants(bool is_extlangs) {
  auto const &extlangs_or_variants = is_extlangs ? m_extended_language_subtags : m_variants;

  for (int idx = 0, num_entries = extlangs_or_variants.size(); idx < num_entries; ++idx)
    if (!validate_one_extlang_or_variant(idx, is_extlangs))
      return false;

  return true;
}

bool
language_c::validate_extensions() {
  if (m_extensions.empty())
    return true;

  if (m_language.empty()) {
    m_parser_error = Y("Extension subtags must follow at least a primary language subtag.");
    return false;
  }

  std::map<std::string, bool> identifiers_seen;

  for (auto const &extension : m_extensions) {
    if (identifiers_seen[extension.identifier]) {
      m_parser_error = Y("Each extension identifier must be used at most once.");
      return false;
    }

    identifiers_seen[extension.identifier] = true;

    // As of 2021-08-07 the IANA language tag extensions registry at
    // https://www.iana.org/assignments/language-tag-extensions-registry/language-tag-extensions-registry
    // only contains the following registered identifiers:
    if (!mtx::included_in(extension.identifier, "t"s, "u"s)) {
      m_parser_error = fmt::format(Y("The value '{0}' is not a registered IANA language tag identifier."), extension.identifier);
      return false;
    }
  }

  return true;
}

language_c
language_c::parse(std::string const &language) {
  init_re();

  language_c l;
  auto language_lower = mtx::string::to_lower_ascii(language);
  auto matches        = s_bcp47_re->match(Q(language_lower));

  if (!matches.hasMatch()) {
    l.m_parser_error = Y("The value does not adhere to the general structure of IETF BCP 47/RFC 5646 language tags.");
    return l;
  }

  // global private use
  if (matches.capturedLength(10)) {
    l.m_private_use = mtx::string::split(to_utf8(matches.captured(10)).substr(1), "-");
    l.m_valid       = true;
    return l;
  }

  if (matches.capturedLength(1) && !l.parse_language(to_utf8(matches.captured(1))))
    return l;

  if (matches.capturedLength(2) && !l.parse_extlangs_or_variants(to_utf8(matches.captured(2)), true))
    return l;

  if (matches.capturedLength(3)) {
    l.m_parser_error = Y("Four-letter language codes are reserved for future use and not supported.");
    return l;
  }

  if (matches.capturedLength(4)) {
    l.m_parser_error = Y("Five- to eight-letter language codes are currently not supported.");
    return l;
  }

  if (matches.capturedLength(5) && !l.parse_script(to_utf8(matches.captured(5))))
    return l;

  if (matches.capturedLength(6) && !l.parse_region(to_utf8(matches.captured(6))))
    return l;

  if (matches.capturedLength(7) && !l.parse_extlangs_or_variants(to_utf8(matches.captured(7)), false))
    return l;

  if (matches.capturedLength(8) && !l.parse_extensions(to_utf8(matches.captured(8))))
    return l;

  if (matches.capturedLength(9))
    l.m_private_use = mtx::string::split(to_utf8(matches.captured(9)).substr(1), "-");

  if (   !l.validate_extlangs_or_variants(true)
      || !l.validate_extlangs_or_variants(false))
    return l;

  l.m_valid = true;

  return l;
}

std::string
language_c::get_iso639_alpha_3_code()
  const noexcept {
  if (!has_valid_iso639_code())
    return {};

  auto language = mtx::iso639::look_up(m_language);
  if (language)
    return language->alpha_3_code;

  return {};
}

std::string
language_c::get_iso639_2_alpha_3_code_or(std::string const &value_if_invalid)
  const noexcept {
  if (!m_valid || m_language.empty())
    return value_if_invalid;

  auto language = mtx::iso639::look_up(m_language);
  if (language && language->is_part_of_iso639_2)
    return language->alpha_3_code;

  return value_if_invalid;
}

language_c &
language_c::set_valid(bool valid) {
  m_valid                = valid;
  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_language(std::string const &language) {
  m_language             = mtx::string::to_lower_ascii(language);
  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_extended_language_subtags(std::vector<std::string> const &extended_language_subtags) {
  m_extended_language_subtags = mtx::string::to_lower_ascii(extended_language_subtags);
  m_formatted_up_to_date      = false;

  return *this;
}

language_c &
language_c::set_script(std::string const &script) {
  m_script               = script;
  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_region(std::string const &region) {
  m_region               = region;
  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_variants(std::vector<std::string> const &variants) {
  m_variants             = mtx::string::to_lower_ascii(variants);
  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_extensions(std::vector<extension_t> const &extensions) {
  m_extensions.clear();
  m_extensions.reserve(extensions.size());

  for (auto const &extension : extensions)
    add_extension(extension);

  return *this;
}

language_c &
language_c::add_extension(extension_t const &extension) {
  std::vector<std::string> extensions_lower;
  extensions_lower.reserve(extension.extensions.size());

  for (auto const &extension_subtag : extension.extensions)
    extensions_lower.emplace_back(mtx::string::to_lower_ascii(extension_subtag));

  auto cleaned_extension = extension_t{ mtx::string::to_lower_ascii(extension.identifier), extensions_lower };
  m_extensions.insert(std::lower_bound(m_extensions.begin(), m_extensions.end(), cleaned_extension), cleaned_extension);

  m_formatted_up_to_date = false;

  return *this;
}

language_c &
language_c::set_private_use(std::vector<std::string> const &private_use) {
  m_private_use          = mtx::string::to_lower_ascii(private_use);
  m_formatted_up_to_date = false;

  return *this;
}

std::string const &
language_c::get_language()
  const noexcept {
  return m_language;
}

std::vector<std::string> const &
language_c::get_extended_language_subtags()
  const noexcept {
  return m_extended_language_subtags;
}

std::string const &
language_c::get_script()
  const noexcept {
  return m_script;
}

std::string const &
language_c::get_region()
  const noexcept {
  return m_region;
}

std::vector<std::string> const &
language_c::get_variants()
  const noexcept {
  return m_variants;
}

std::vector<language_c::extension_t> const &
language_c::get_extensions()
  const noexcept {
  return m_extensions;
}

std::vector<std::string> const &
language_c::get_private_use()
  const noexcept {
  return m_private_use;
}

bool
language_c::operator ==(language_c const &other)
  const noexcept {
  return format() == other.format();
}

bool
language_c::operator !=(language_c const &other)
  const noexcept {
  return format() != other.format();
}

bool
language_c::matches(language_c const &match)
  const noexcept {
  if (!is_valid() || !match.is_valid())
    return false;

  if (!match.m_language.empty() && (m_language != match.m_language))
    return false;

  if (!match.m_extended_language_subtags.empty() && (m_extended_language_subtags != match.m_extended_language_subtags))
    return false;

  if (!match.m_script.empty() && (m_script != match.m_script))
    return false;

  if (!match.m_region.empty() && (m_region != match.m_region))
    return false;

  if (!match.m_variants.empty() && (m_variants != match.m_variants))
    return false;

  if (!match.m_extensions.empty() && (m_extensions != match.m_extensions))
    return false;

  if (!match.m_private_use.empty() && (m_private_use != match.m_private_use))
    return false;

  return true;
}

language_c
language_c::find_best_match(std::vector<language_c> const &potential_matches)
  const noexcept {
  language_c best_match;
  auto num_components_best_match = 0;

  for (auto const &potential_match : potential_matches) {
    if (!matches(potential_match))
      continue;

    auto num_components = 1;
    auto formatted      = potential_match.format();

    for (auto const &chr : formatted)
      if (chr == '-')
        ++num_components;

    if (num_components > num_components_best_match) {
      best_match                = potential_match;
      num_components_best_match = num_components;
    }
  }

  return best_match;
}

void
language_c::disable() {
  ms_disabled = true;
}

bool
language_c::is_disabled() {
  return ms_disabled;
}

} // namespace mtx::bcp47
