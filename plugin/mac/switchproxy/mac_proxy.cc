//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

#include "npswitchproxy.h"

static const char* const kNetworkSetupPath = "/sbin/networksetup";

MacProxy::MacProxy() : authorization_(NULL) {
}

MacProxy::~MacProxy() {
  if (authorization_) {
    AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
  }
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
  return true;
}

void MacProxy::PlatformDependentShutdown() {
}

// static
char* MacProxy::CreateCStringFromString(CFStringRef str) {
  CFIndex len = CFStringGetLength(str);
  CFIndex max_len =
      CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingASCII) + 1;
  char *cstr = new char(max_len);
  if (CFStringGetCString(str, cstr, max_len, kCFStringEncodingASCII)) {
    return cstr;
  }
  delete cstr;
  return NULL;
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
char* MacProxy::CopyCStringFromDictionary(
    CFDictionaryRef dict,
    CFStringRef key) {
  CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
    return NULL;
  }
  return MacProxy::CreateCStringFromString((CFStringRef) value);
}

// static
bool MacProxy::IsNetworkInterfaceActive(SCNetworkInterfaceRef net_if) {
  CFStringRef bsd_name = SCNetworkInterfaceGetBSDName(net_if);
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  SCDynamicStoreRef dynamic_store = SCDynamicStoreCreate(
      kCFAllocatorDefault,
      CFSTR("Chrome Switch Proxy Plugin"),
      NULL,
      &context);
  CFMutableStringRef path = CFStringCreateMutable(kCFAllocatorDefault, 0);
  CFStringAppendFormat(path, NULL,
                       CFSTR("State:/Network/Interface/%@/Link"),
                       bsd_name);
  CFPropertyListRef dict = SCDynamicStoreCopyValue(dynamic_store, path);
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
  CFRelease(dynamic_store);
  return active; 
}

// static
SCNetworkServiceRef MacProxy::CopyActiveNetworkService(
    SCNetworkSetRef network_set) {
  CFArrayRef services = SCNetworkSetCopyServices(network_set);
  int arraySize = CFArrayGetCount(services);
  SCNetworkServiceRef active_network_service = NULL;
  for (int i = 0; i < arraySize; ++i) {
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
          active_network_service = (SCNetworkServiceRef)CFRetain(service);
          break;
        }
      }
    }
  }
  CFRelease(services);
  return active_network_service;
}

// static
bool MacProxy::GetActiveConnectionName(const char** connection_name) {
  bool result = false;
  SCPreferencesRef preference = SCPreferencesCreate(
      kCFAllocatorDefault, CFSTR("Chrome Switch Proxy Plugin"), NULL);
  SCNetworkSetRef network_set = SCNetworkSetCopyCurrent(preference);
  if (!network_set) {
    CFRelease(preference);
    return false;
  }
  SCNetworkServiceRef service =
      MacProxy::CopyActiveNetworkService(network_set);
  SCNetworkInterfaceRef net_if = NULL;
  if (service) {
    net_if = SCNetworkServiceGetInterface(service);
  }
  if (net_if) {
    CFStringRef connection_name_ref =
        SCNetworkInterfaceGetLocalizedDisplayName(net_if);
    *connection_name =
        MacProxy::CreateCStringFromString(connection_name_ref);
    
    result = true;
  }
  if (service) {
    CFRelease(service);
  }
  CFRelease(network_set);
  CFRelease(preference);
  DebugLog("Get connection name: %s\n", *connection_name);
  return result;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  SCDynamicStoreRef dynamic_store = SCDynamicStoreCreate(
      kCFAllocatorDefault,
      CFSTR("Chrome Switch Proxy Plugin"),
      NULL,
      &context);
  CFDictionaryRef proxies = SCDynamicStoreCopyProxies(dynamic_store);
  config->auto_detect = MacProxy::GetBoolFromDictionary(
      proxies,
      kSCPropNetProxiesProxyAutoDiscoveryEnable,
      false);
  config->auto_config = MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesProxyAutoConfigEnable, false);
  config->auto_config_url = MacProxy::CopyCStringFromDictionary(
      proxies, kSCPropNetProxiesProxyAutoConfigURLString);
  CFMutableArrayRef array =
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
  if (MacProxy::GetBoolFromDictionary(
      proxies, kSCPropNetProxiesHTTPEnable, false)) {
    char* http_proxy_host = MacProxy::CopyCStringFromDictionary(
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
    char* https_proxy_host = MacProxy::CopyCStringFromDictionary(
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
    char* ftp_proxy_host = MacProxy::CopyCStringFromDictionary(
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
    char* socks_proxy_host = MacProxy::CopyCStringFromDictionary(
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
        kCFAllocatorDefault, array, CFSTR(" "));
    config->proxy_server =
        MacProxy::CreateCStringFromString(proxy_setting);
    CFRelease(proxy_setting);
  }
  DebugLog("Get config, auto_detect = %d, auto_config=%d, auto_config_url=%s\n",
           config->auto_detect, config->auto_config, config->auto_config_url);
  DebugLog("proxy:%s\n", config->proxy_server);

  CFRelease(array);
  CFRelease(dynamic_store);
  return true;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  SCPreferencesRef sc_preference = SCPreferencesCreate(
      kCFAllocatorDefault, CFSTR("Chrome Switch Proxy Plugin"), NULL);
  SCNetworkSetRef network_set = SCNetworkSetCopyCurrent(sc_preference);
  if (!network_set) {
    CFRelease(sc_preference);
    return false;
  }
  SCNetworkServiceRef service =
      MacProxy::CopyActiveNetworkService(network_set);
  if (!service || !GetAuthorizationForRootPrivilege()) {
    CFRelease(network_set);
    CFRelease(sc_preference);
    return false;
  }
  
  CFRelease(service);
  CFRelease(network_set);
  CFRelease(sc_preference);
  return true;
}
