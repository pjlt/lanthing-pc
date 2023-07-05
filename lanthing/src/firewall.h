#pragma once
#include <string>

namespace lt {

void create_inbound_firewall_rule(const std::string& rule_name, const std::string& path);

} // namespace lt