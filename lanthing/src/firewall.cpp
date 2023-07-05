#include "firewall.h"

#include <g3log/g3log.hpp>

#if defined(LT_WINDOWS)
#include <atlcomcli.h>
#include <netfw.h>
#include <wrl/client.h>
#endif // LT_WINDOWS

#include <ltlib/strings.h>

namespace lt {

using Microsoft::WRL::ComPtr;

#if defined(LT_WINDOWS)
void create_inbound_firewall_rule(const std::string& _rule_name, const std::string& _path) {
    std::wstring rule_name = ltlib::utf8_to_utf16(_rule_name);
    std::wstring path = ltlib::utf8_to_utf16(_path);
    bool com_inited = false;
    do {
        HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (hr != RPC_E_CHANGED_MODE) {
            if (FAILED(hr)) {
                LOG(WARNING) << "CoInitializeEx failed";
                return;
            }
        }
        com_inited = true;
        ComPtr<INetFwPolicy2> policy;
        hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER,
                              __uuidof(INetFwPolicy2), (void**)&policy);
        if (FAILED(hr)) {
            LOG(WARNING) << "CoCreateInstance NetFwPolicy2 failed";
            break;
        }
        ComPtr<INetFwRules> rules;
        hr = policy->get_Rules(&rules);
        if (FAILED(hr)) {
            LOG(WARNING) << "INetFwPolicy2 get_Rules failed";
            break;
        }
        IEnumVARIANT* enum_variant;
        IUnknown* enumerator;
        CComVariant com_var;
        hr = rules->get__NewEnum(&enumerator);
        if (enumerator == nullptr) {
            LOG(WARNING) << "INetFwRules NewEnum failed";
            break;
        }
        hr = enumerator->QueryInterface(__uuidof(IEnumVARIANT), (void**)&enum_variant);
        while (SUCCEEDED(hr) && hr != S_FALSE) {
            INetFwRule* rule;
            ULONG fetched = 0;
            hr = enum_variant->Next(1, &com_var, &fetched);
            if (S_FALSE != hr) {
                if (SUCCEEDED(hr)) {
                    hr = com_var.ChangeType(VT_DISPATCH);
                }
                if (SUCCEEDED(hr)) {
                    hr =
                        (V_DISPATCH(&com_var))
                            ->QueryInterface(__uuidof(INetFwRule), reinterpret_cast<void**>(&rule));
                }

                if (SUCCEEDED(hr)) {
                    BSTR temp_bstr = nullptr;
                    hr = rule->get_ApplicationName(&temp_bstr);
                    if (SUCCEEDED(hr) && temp_bstr != nullptr) {
                        if (wcscmp(temp_bstr, (BSTR)path.c_str()) == 0) {
                            LOG(WARNING) << "The firewall rule with program '" << _path
                                         << "' already exists";
                            goto CLEANUP;
                        }
                    }
                }
            }
        }
        ComPtr<INetFwRule> rule;
        hr = CoCreateInstance(__uuidof(NetFwRule), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwRule),
                              (void**)rule.GetAddressOf());
        if (FAILED(hr)) {
            LOG(WARNING) << "CoCreateInstance NetFwRule failed";
            break;
        }
        rule->put_ApplicationName((BSTR)path.c_str());
        rule->put_Name((BSTR)rule_name.c_str());
        rule->put_Profiles(NET_FW_PROFILE2_ALL);
        rule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
        rule->put_Action(NET_FW_ACTION_ALLOW);
        rule->put_Enabled(VARIANT_TRUE);
        hr = rules->Add(rule.Get());
        if (FAILED(hr)) {
            LOG(WARNING) << "INetFwRules Add failed";
            break;
        }
        LOG(WARNING) << "INetFwRules Add success";
    } while (0);
CLEANUP:
    if (com_inited) {
        CoUninitialize();
    }
}
#else  // LT_WINDOWS
void create_inbound_firewall_rule(const std::string& rule_name, const std::string& path) {
    (void)rule_name;
    (void)path;
}
#endif // LT_WINDOWS

} // namespace lt