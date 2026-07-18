#include "autopoiesis/feature_request.hpp"

#include <cctype>

namespace apo {
namespace {
bool non_empty_string(const json& value) {
  if (!value.is_string()) return false;
  for (const auto character : value.get<std::string>()) {
    if (!std::isspace(static_cast<unsigned char>(character))) return true;
  }
  return false;
}

bool non_empty_strings(const json& value) {
  if (!value.is_array() || value.empty()) return false;
  for (const auto& item : value) {
    if (!non_empty_string(item)) return false;
  }
  return true;
}

bool require_string(const json& object, const char* field, std::string& error) {
  if (object.contains(field) && non_empty_string(object[field])) return true;
  error = std::string("champ requis invalide : ") + field;
  return false;
}

bool require_strings(const json& object, const char* field, std::string& error) {
  if (object.contains(field) && non_empty_strings(object[field])) return true;
  error = std::string("liste requise invalide : ") + field;
  return false;
}
}

bool validate_feature_request(const json& request, std::string& error) {
  error.clear();
  if (!request.is_object()) {
    error = "la demande doit etre un objet";
    return false;
  }
  if (!request.contains("requested") || !request["requested"].is_boolean() || !request["requested"].get<bool>()) {
    error = "la demande doit etre explicitement demandee";
    return false;
  }
  for (const char* field : {"evolution_key", "domain", "title", "need", "obstacle", "proposed_change"}) {
    if (!require_string(request, field, error)) return false;
  }
  if (!request.contains("mechanism") || !request["mechanism"].is_object()) {
    error = "champ requis invalide : mechanism";
    return false;
  }
  const auto& mechanism = request["mechanism"];
  for (const char* field : {"name", "summary"}) {
    if (!require_string(mechanism, field, error)) return false;
  }
  for (const char* field : {"resources", "actions", "preconditions", "deterministic_effects"}) {
    if (!require_strings(mechanism, field, error)) return false;
  }
  return require_strings(request, "acceptance_tests", error);
}
}
