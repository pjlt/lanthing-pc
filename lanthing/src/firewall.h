#pragma once
#include <string>

namespace lt {

void createInboundFirewallRule(const std::string& rule_name, const std::string& path);

} // namespace lt