//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

#include "npswitchproxy.h"

MacProxy::MacProxy() : authorization_(NULL) {
}

MacProxy::~MacProxy() {
  AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
}

bool MacProxy::GetAuthorizationForRootPrivilege() {
  OSStatus status;
  if (!authorization_) {
    status = AuthorizationCreate(NULL,
                                 kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults,
                                 &authorization_);
    if (status != errAuthorizationSuccess) {
      return false;
    }
  }
  AuthorizationItem right = {kAuthorizationRightExecute, 0, NULL, 0};
  AuthorizationRights rights = {1, &right};
  AuthorizationFlags flags = kAuthorizationFlagDefaults |
                             kAuthorizationFlagInteractionAllowed |
                             kAuthorizationFlagPreAuthorize |
                             kAuthorizationFlagExtendRights;
  status = AuthorizationCopyRights(
      authorization_, &rights, NULL, flags, NULL);
  if (status != errAuthorizationSuccess) {
    return false;
  }
  return true;
}

bool MacProxy::PlatformDependentStartup() {
  sc_preference_ = SCPreferencesCreate(
      kCFAllocatorDefault, CFSTR("Chrome Switch Proxy Plugin"), NULL);
  return true;
}

void MacProxy::PlatformDependentShutdown() {
  CFRelease(sc_preference_);
}

// static
int MacProxy::NumberOfBytesInCFString(CFStringRef str) {
  CFIndex len = CFStringGetLength(str);
  CFIndex used_bytes = 0;
  CFStringGetBytes(str, CFRangeMake(0, len), kCFStringEncodingUTF8,
                   0, false, NULL, len * 4 + 1, &used_bytes);
  return (int)used_bytes;
}

// static
CFStringRef MacProxy::CreateCFStringFromUtf8String(char* utf8_str) {
  return CFStringCreateWithBytes(
      kCFAllocatorDefault,
      (const UInt8*)utf8_str,
      strlen(utf8_str),
      kCFStringEncodingUTF8,
      false);
}

// static
char* MacProxy::CreateUtf8StringFromString(CFStringRef str) {
  int len = MacProxy::NumberOfBytesInCFString(str);
  UInt8 *utf8_str = new UInt8(len + 1);
  CFStringGetBytes(str,
                   CFRangeMake(0, CFStringGetLength(str)),
                   kCFStringEncodingUTF8,
                   0,
                   false,
                   utf8_str,
                   len,
                   NULL);
  utf8_str[len] = 0;
  return (char*)utf8_str;
}

// static
bool MacProxy::GetBoolFromDictionary(
    CFDictionaryRef dict,
    CFStringRef key,
    bool default_value) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
    return default_value;
  }
  int int_value;
  if (CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &int_value)) {
    return int_value;
  }
  return default_value;
}

int MacProxy::GetIntFromDictionary(
    CFDictionaryRef dict,
    CFStringRef key,
    int default_value) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
    return default_value;
  }
  int int_value;
  if (CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &int_value)) {
    return int_value;
  }
  return default_value;
}

// static
char* MacProxy::CopyUtf8StringFromDictionary(
    CFDictionaryRef dict,
    CFStringRef key) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
    return NULL;
  }
  return MacProxy::CreateUtf8StringFromString((CFStringRef) value);
}

// static
bool MacProxy::IsNetworkInterfaceActive(SCNetworkInterfaceRef net_if) {
  CFStringRef bsd_name = SCNetworkInterfaceGetBSDName(net_if);
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  SCDynamicStoreRef sc_store = SCDynamicStoreCreate(
      kCFAllocatorDefault,
      CFSTR("Chrome Switch Proxy Plugin"),
      NULL,
      &context);
  CFMutableStringRef path = CFStringCreateMutable(kCFAllocatorDefault, 0);
  CFStringAppendFormat(path, NULL,
                       CFSTR("State:/Network/Interface/%@/Link"),
                       bsd_name);
  CFPropertyListRef dict = SCDynamicStoreCopyValue(sc_store, path);
  bool active = false;
  if (dict && CFGetTypeID(dict) == CFDictionaryGetTypeID() &&
      CFDictionaryGetValue((CFDictionaryRef)dict, CFSTR("Active")) ==
      kCFBooleanTrue) {
    active = true;
  }
  if (dict) {
    CFRelease(dict);
  }
  CFRelease(path);
  CFRelease(sc_store);
  return active; 
}

// static
SCNetworkInterfaceRef MacProxy::GetActiveNetworkInterface(
    SCNetworkSetRef network_set) {
  CFArrayRef services = SCNetworkSetCopyServices(network_set);
  int arraySize = CFArrayGetCount(services);
  SCNetworkInterfaceRef active_net_if = NULL;
  for (int i = 0; i < arraySize; i++) {
    SCNetworkServiceRef service =
        (SCNetworkServiceRef) CFArrayGetValueAtIndex(services, i);
    if (service && SCNetworkServiceGetEnabled(service)) {
      SCNetworkInterfaceRef net_if = SCNetworkServiceGetInterface(service);
      CFStringRef if_type = SCNetworkInterfaceGetInterfaceType(net_if);
      if (CFStringCompare(if_type, CFSTR("Ethernet"), 0) ==
          kCFCompareEqualTo ||
          CFStringCompare(if_type, CFSTR("IEEE80211"), 0) ==
          kCFCompareEqualTo) {
        // Check if the network status is active.
        if (MacProxy::IsNetworkInterfaceActive(net_if)) {
          active_net_if = net_if;
          break;
        }
      }
    }
  }
  CFRelease(services);
  return active_net_if;
}

// static
bool MacProxy::GetActiveConnectionName(const char** connection_name) {
  bool result = false;
  SCNetworkSetRef network_set = SCNetworkSetCopyCurrent(sc_preference_);
  if (!network_set) {
    return false;
  }
  SCNetworkInterfaceRef net_if =
      MacProxy::GetActiveNetworkInterface(network_set);
  if (net_if) {
    CFMutableArrayRef names =
        CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    CFStringRef current_network_set_name = SCNetworkSetGetName(network_set);
    if (current_network_set_name) {
      CFArrayAppendValue(names, current_network_set_name);
    }
    CFStringRef current_net_if_name =
        SCNetworkInterfaceGetLocalizedDisplayName(net_if);
    if (current_net_if_name) {
      CFArrayAppendValue(names, current_net_if_name);
    }
    CFStringRef connection_name_ref = CFStringCreateByCombiningStrings(
        kCFAllocatorDefault, names, CFSTR("::"));
    *connection_name =
        MacProxy::CreateUtf8StringFromString(connection_name_ref);
    CFRelease(names);
    result = true;
  }
  CFRelease(network_set);
  DebugLog("Get connection name: %s\n", *connection_name);
  return result;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  SCDynamicStoreRef sc_store = SCDynamicStoreCreate(
      kCFAllocatorDefault,
      CFSTR("Chrome Switch Proxy Plugin"),
      NULL,
      &context);
  CFDictionaryRef proxies = SCDynamicStoreCopyProxies(sc_store);
  config->auto_detect = MacProxy::GetBoolFromDictionary(
      proxies,
      kSCPropNetProxiesProxyAutoDiscoveryEnable,
      false);
  config->auto_config = MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesProxyAutoConfigEnable, false);
  config->auto_config_url = MacProxy::CopyUtf8StringFromDictionary(
      proxies, kSCPropNetProxiesProxyAutoConfigURLString);
  CFMutableArrayRef array =
      CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
  if (MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesHTTPEnable, false)) {
    char* http_proxy_host = MacProxy::CopyUtf8StringFromDictionary(
        proxies, kSCPropNetProxiesHTTPProxy);
    int http_proxy_port = MacProxy::GetIntFromDictionary(
        proxies, kSCPropNetProxiesHTTPPort, 80);
    CFStringRef proxy = CFStringCreateWithFormat(
        kCFAllocatorDefault, NULL, CFSTR("http=%s:%d;"), http_proxy_host,
        http_proxy_port);
    CFArrayAppendValue(array, proxy);
    CFRelease(proxy);
  }
  if (MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesHTTPSEnable, false)) {
    char* https_proxy_host = MacProxy::CopyUtf8StringFromDictionary(
        proxies, kSCPropNetProxiesHTTPSProxy);
    int https_proxy_port = MacProxy::GetIntFromDictionary(
        proxies, kSCPropNetProxiesHTTPSPort, 80);
    CFStringRef proxy = CFStringCreateWithFormat(
        kCFAllocatorDefault, NULL, CFSTR("https=%s:%d;"), https_proxy_host,
        https_proxy_port);
    CFArrayAppendValue(array, proxy);
    CFRelease(proxy);
  }
  if (MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesFTPEnable, false)) {
    char* ftp_proxy_host = MacProxy::CopyUtf8StringFromDictionary(
        proxies, kSCPropNetProxiesFTPProxy);
    int ftp_proxy_port = MacProxy::GetIntFromDictionary(
        proxies, kSCPropNetProxiesFTPPort, 80);
    CFStringRef proxy = CFStringCreateWithFormat(
        kCFAllocatorDefault, NULL, CFSTR("ftp=%s:%d;"), ftp_proxy_host,
        ftp_proxy_port);
    CFArrayAppendValue(array, proxy);
    CFRelease(proxy);
  }
  if (MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesSOCKSEnable, false)) {
    char* socks_proxy_host = MacProxy::CopyUtf8StringFromDictionary(
        proxies, kSCPropNetProxiesSOCKSProxy);
    int socks_proxy_port = MacProxy::GetIntFromDictionary(
        proxies, kSCPropNetProxiesSOCKSPort, 1080);
    CFStringRef proxy = CFStringCreateWithFormat(
        kCFAllocatorDefault, NULL, CFSTR("socks=%s:%d;"), socks_proxy_host,
        socks_proxy_port);
    CFArrayAppendValue(array, proxy);
    CFRelease(proxy);
  }
  if (CFArrayGetCount(array) > 0) {
    config->use_proxy = true;
    CFStringRef proxy_setting = CFStringCreateByCombiningStrings(
        kCFAllocatorDefault, array, CFSTR(""));
    config->proxy_server = MacProxy::CreateUtf8StringFromString(proxy_setting);
    CFRelease(proxy_setting);
  }
  DebugLog("Get config, auto_detect = %d, auto_config=%d, auto_config_url=%s\n",
           config->auto_detect, config->auto_config, config->auto_config_url);
  DebugLog("proxy:%s\n", config->proxy_server);

  CFRelease(array);
  CFRelease(sc_store);
  return true;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  if (!GetAuthorizationForRootPrivilege()) {
    return false;
  }
  return true;
}
