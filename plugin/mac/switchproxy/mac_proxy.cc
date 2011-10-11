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
    CFArrayAppendValue(names, current_network_set_name);
    CFStringRef current_net_if_name =
        SCNetworkInterfaceGetLocalizedDisplayName(net_if);
    CFArrayAppendValue(names, current_net_if_name);
    CFStringRef connection_name_ref = CFStringCreateByCombiningStrings(
        kCFAllocatorDefault, names, CFSTR("::"));
    int len = MacProxy::NumberOfBytesInCFString(connection_name_ref);
    UInt8 *utf8_name = new UInt8(len + 1);
    CFStringGetBytes(connection_name_ref,
                     CFRangeMake(0, CFStringGetLength(connection_name_ref)),
                     kCFStringEncodingUTF8,
                     0,
                     false,
                     utf8_name,
                     len,
                     NULL);
    utf8_name[len] = 0;
    *connection_name = (const char*) utf8_name;
    CFRelease(names);
    result = true;
  }
  CFRelease(network_set);
  return result;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  bool result = false;
  SCNetworkSetRef network_set = SCNetworkSetCopyCurrent(sc_preference_);
  if (!network_set) {
    return false;
  }
  // TODO(wzzhu): Get proxy configuration from sc.
  result = true;
  CFRelease(network_set);
  return result;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  if (!GetAuthorizationForRootPrivilege()) {
    return false;
  }
  return true;
}
