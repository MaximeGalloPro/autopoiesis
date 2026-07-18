#include "autopoiesis/openai_client.hpp"

#include <cassert>
#include <string>

using namespace apo;

int main() {
  const std::string body = R"({"error":{"type":"rate_limit_error","code":"rate_limit_exceeded","message":"Limite atteinte pour sk-proj-secret123"},"ignored":"do-not-log"})";
  const auto diagnostic = api_http_diagnostic(429, body, 1234);

  assert(diagnostic.find("HTTP 429") != std::string::npos);
  assert(diagnostic.find("rate_limit_error") != std::string::npos);
  assert(diagnostic.find("rate_limit_exceeded") != std::string::npos);
  assert(diagnostic.find("1.23 s") != std::string::npos);
  assert(diagnostic.find("[SECRET MASQUE]") != std::string::npos);
  assert(diagnostic.find("sk-proj-secret123") == std::string::npos);
  assert(diagnostic.find("do-not-log") == std::string::npos);

  const auto malformed = api_http_diagnostic(400, "not-json", 25);
  assert(malformed.find("HTTP 400") != std::string::npos);
  assert(malformed.find("corps d'erreur illisible") != std::string::npos);
}
