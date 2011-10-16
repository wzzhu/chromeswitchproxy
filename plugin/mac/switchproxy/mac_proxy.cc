//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

#include "npswitchproxy.h"

static const char* const kNetworkSetupPath = "/usr/sbin/networksetup";
static int const kMaxCommandArgumentLength = 128;
static int const kMaxArgNum = 5;
static int const kMaxPortLength = 16;

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

OSStatus MacProxy::RunNetworkSetupCommand(char *const *args) {
  int state;
  OSStatus status = AuthorizationExecuteWithPrivileges(
      authorization_, kNetworkSetupPath, kAuthorizationFlagDefaults,
      args, NULL);
  wait(&state);
  return status;
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
  char *cstr = new char[max_len];
  if (CFStringGetCString(str, cstr, max_len, kCFStringEncodingASCII)) {
    return cstr;
  }
  delete [] cstr;
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
  CFRelease(proxies);
  CFRelease(dynamic_store);
  return true;
}

void MacProxy::ParseProxyLine(const char* start_proxy, const char* end_proxy,
                              char* proxy, char* port) {
  int i = 0;
  bool begin_port = false;
  while(start_proxy < end_proxy && isspace(*start_proxy)) {
    ++start_proxy;
  }
  while(start_proxy < end_proxy && isspace(*end_proxy)) {
    --end_proxy;
  }
  while (start_proxy <= end_proxy) {
    if (begin_port) {
      if (i < kMaxPortLength) {
        port[i] = *start_proxy;
      }
    } else {
      if (*start_proxy == ':') {
        i = -1;
        begin_port = true;
      } else {
        if (i < kMaxCommandArgumentLength) {
          proxy[i] = *start_proxy;
        }
      }
    }
    ++start_proxy;
    ++i;
  }
}

void MacProxy::ParseProxyServerDescription(
    const char* proxy_server, char* proxies[4], char* ports[4]) {
  const char *ptr_base = proxy_server;
  while (*ptr_base && (isspace(*ptr_base) || *ptr_base == ';')) {
    ++ptr_base;
  }
  const char *ptr = ptr_base;
  while (*ptr) {
    const char *end_ptr = strchr(ptr, ';');
    if (!end_ptr) {
      end_ptr = &(ptr[strlen(ptr)]);
    }
    const char *separator = strchr(ptr, '=');
    const char *start_proxy = ptr;
    const char *end_proxy = separator;
    if (end_proxy) {
      --end_proxy;
      while(start_proxy < end_proxy && isspace(*start_proxy)) {
        ++start_proxy;
      }
      while(start_proxy < end_proxy && isspace(*end_proxy)) {
        --end_proxy;
      }
      int index = -1;
      int len = end_proxy - start_proxy + 1;
      if (len == 5 && strncmp("https", start_proxy, len) == 0) {
        index = 1;
      } else if (len == 4 && strncmp("http", start_proxy, len) == 0) {
        index = 0;
      } else if (len == 3 && strncmp("ftp", start_proxy, len) == 0) {
        index = 2;
      } else if (len == 5 && strncmp("socks", start_proxy, len) == 0 ) {
        index = 3;
      }
      if (index >= 0) {
        ParseProxyLine(separator + 1, end_ptr - 1, proxies[index],
                       ports[index]);
      }
    } else {
      // Fill in the rest unfilled proxy entries with the proxy, except socks.
      for (int index = 0; index < 3; ++index ) {
        if (strlen(proxies[index]) == 0) {
          ParseProxyLine(start_proxy, end_ptr - 1, proxies[index], ports[index]);
        }
      }
    }
    if (*end_ptr) {
      ptr = end_ptr + 1;
    } else {
      ptr = end_ptr;
    }
  }
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
  CFStringRef service_name = SCNetworkServiceGetName(service);
  char *service_name_str =
      MacProxy::CreateCStringFromString(service_name);
  char *args[kMaxArgNum];
  args[0] = new char[kMaxCommandArgumentLength + 1];
  args[1] = new char[kMaxCommandArgumentLength + 1];
  args[2] = new char[kMaxCommandArgumentLength + 1];
  for (int i = 3; i < kMaxArgNum; ++i) {
    args[i] = NULL;
  }
  for (int i = 0; i < 3; ++i) {
    bzero(args[i], kMaxCommandArgumentLength);
  }
  strncpy(args[1], service_name_str, kMaxCommandArgumentLength);

  strncpy(args[0], "-setautoproxystate", kMaxCommandArgumentLength);
  if (config.auto_config && config.use_proxy) {
    strncpy(args[2], "on", kMaxCommandArgumentLength);
  } else {
    strncpy(args[2], "off", kMaxCommandArgumentLength);
  }
  RunNetworkSetupCommand(args);
  strncpy(args[0], "-setautoproxyurl", kMaxCommandArgumentLength);
  if (config.auto_config_url) {
    strncpy(args[2], config.auto_config_url, kMaxCommandArgumentLength);
  } else {
    strncpy(args[2], "", kMaxCommandArgumentLength);
  }
  RunNetworkSetupCommand(args);
  if (!config.use_proxy) {
    strncpy(args[0], "-setwebproxystate", kMaxCommandArgumentLength);
    strncpy(args[2], "off", kMaxCommandArgumentLength);
    RunNetworkSetupCommand(args);
    strncpy(args[0], "-setsecurewebproxystate", kMaxCommandArgumentLength);
    strncpy(args[2], "off", kMaxCommandArgumentLength);
    RunNetworkSetupCommand(args);
    strncpy(args[0], "-setftpproxystate", kMaxCommandArgumentLength);
    strncpy(args[2], "off", kMaxCommandArgumentLength);
    RunNetworkSetupCommand(args);
    strncpy(args[0], "-setsocksfirewallproxystate", kMaxCommandArgumentLength);
    strncpy(args[2], "off", kMaxCommandArgumentLength);
    RunNetworkSetupCommand(args);
  } else {
    char *proxies[4];
    char *ports[4];
    for (int i = 0; i < 4; ++i) {
      proxies[i] = new char[kMaxCommandArgumentLength + 1];
      ports[i] = new char[kMaxPortLength + 1];
      bzero(proxies[i], kMaxCommandArgumentLength + 1);
      bzero(ports[i], kMaxPortLength + 1);
    }
    ParseProxyServerDescription(config.proxy_server, proxies, ports);
    for (int i = 0; i < 4; ++i ) {
      if (strlen(proxies[i])) {
        switch (i) {
          case 0:
            strncpy(args[0], "-setwebproxystate", kMaxCommandArgumentLength);
            strncpy(args[2], "on", kMaxCommandArgumentLength);
            RunNetworkSetupCommand(args);
            strncpy(args[0], "-setwebproxy", kMaxCommandArgumentLength);
            strncpy(args[2], proxies[i], kMaxCommandArgumentLength);
            break;
          case 1:
            strncpy(args[0], "-setsecurewebproxystate",
                    kMaxCommandArgumentLength);
            strncpy(args[2], "on", kMaxCommandArgumentLength);
            RunNetworkSetupCommand(args);
            strncpy(args[0], "-setsecurewebproxy", kMaxCommandArgumentLength);
            strncpy(args[2], proxies[i], kMaxCommandArgumentLength);
            break;
          case 2:
            strncpy(args[0], "-setftpproxystate", kMaxCommandArgumentLength);
            strncpy(args[2], "on", kMaxCommandArgumentLength);
            RunNetworkSetupCommand(args);
            strncpy(args[0], "-setftpproxy", kMaxCommandArgumentLength);
            strncpy(args[2], proxies[i], kMaxCommandArgumentLength);
            break;
          case 3:
            strncpy(args[0], "-setsocksfirewallproxystate",
                    kMaxCommandArgumentLength);
            strncpy(args[2], "on", kMaxCommandArgumentLength);
            RunNetworkSetupCommand(args);
            strncpy(args[0], "-setsocksfirewallproxy",
                    kMaxCommandArgumentLength);
            strncpy(args[2], proxies[i], kMaxCommandArgumentLength);
            break;
        }
        args[3] = new char[kMaxPortLength + 1];
        strncpy(args[3], ports[i], kMaxPortLength);
        RunNetworkSetupCommand(args);
        delete [] args[3];
        args[3] = NULL;
      }
    }
    for (int i = 0; i < 4; ++i) {
      delete [] proxies[i];
      delete [] ports[i];
    }
  }
  delete [] args[0];
  delete [] args[1];
  delete [] args[2];
  delete service_name_str;
  CFRelease(service);
  CFRelease(network_set);
  CFRelease(sc_preference);
  return true;
}
