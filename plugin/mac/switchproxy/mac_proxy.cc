//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

#include "npswitchproxy.h"

#include <SystemConfiguration/SystemConfiguration.h>

MacProxy::MacProxy() : authorizationRef(NULL) {
}

MacProxy::~MacProxy() {
  AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
}

bool MacProxy::GetAuthorizationForRootPrivilege() {
  OSStatus status;
  if (!authorizationRef) {
    status = AuthorizationCreate(NULL,
                                 kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults,
                                 &authorizationRef);
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
      authorizationRef, &rights, NULL, flags, NULL);
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
int MacProxy::NumberOfBytesInCFString(CFStringRef string_ref) {
  CFIndex len = CFStringGetLength(string_ref);
  CFIndex used_bytes = 0;
  CFStringGetBytes(string_ref, CFRangeMake(0, len), kCFStringEncodingUTF8,
                   0, false, NULL, len * 4 + 1, &used_bytes);
  return (int)used_bytes;
}

// static
bool MacProxy::IsNetworkInterfaceActive(CFStringRef if_bsd_name) {
  SCDynamicStoreContext context = {0, NULL, NULL, NULL, NULL};
  SCDynamicStoreRef store_ref = SCDynamicStoreCreate(
      kCFAllocatorDefault,
      CFSTR("Chrome Switch Proxy Plugin"),
      NULL,
      &context);
  CFMutableStringRef path = CFStringCreateMutable(kCFAllocatorDefault, 0);
  CFStringAppendFormat(path, NULL,
                       CFSTR("State:/Network/Interface/%@/Link"),
                       if_bsd_name);
  CFPropertyListRef dict = SCDynamicStoreCopyValue(store_ref, path);
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
  CFRelease(store_ref);
  return active; 
}

bool MacProxy::GetActiveConnectionName(const char** connection_name) {
  SCPreferencesRef preference_ref = SCPreferencesCreate(
      kCFAllocatorDefault, CFSTR("Chrome Switch Proxy Plugin"), NULL);
  SCNetworkSetRef network_ref = SCNetworkSetCopyCurrent(preference_ref);
  if (!network_ref) {
    CFRelease(preference_ref);
    return false;
  }
  CFArrayRef services = SCNetworkSetCopyServices(network_ref);
  int arraySize = CFArrayGetCount(services);
  CFStringRef active_if_name_ref = NULL;
  for (int i = 0; i < arraySize; i++) {
    SCNetworkServiceRef service =
        (SCNetworkServiceRef) CFArrayGetValueAtIndex(services, i);
    if (service && SCNetworkServiceGetEnabled(service)) {
      SCNetworkInterfaceRef if_ref = SCNetworkServiceGetInterface(service);
      CFStringRef type_ref = SCNetworkInterfaceGetInterfaceType(if_ref);
      if (CFStringCompare(type_ref, CFSTR("Ethernet"), 0) ==
          kCFCompareEqualTo ||
          CFStringCompare(type_ref, CFSTR("IEEE80211"), 0) ==
          kCFCompareEqualTo) {
        // Check if the network status is active.
        CFStringRef bsd_name = SCNetworkInterfaceGetBSDName(if_ref);
        if (MacProxy::IsNetworkInterfaceActive(bsd_name)) {
          active_if_name_ref =
              CFStringCreateCopy(kCFAllocatorDefault,
                                 SCNetworkServiceGetName(service));
          break;
        }
      }
    }
  }

  CFRelease(services);
  CFRelease(preference_ref);
  if (active_if_name_ref) {
    CFMutableArrayRef names =
        CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
    CFStringRef current_network_set_name_ref = SCNetworkSetGetName(network_ref);
    CFArrayAppendValue(names, current_network_set_name_ref);
    CFArrayAppendValue(names, active_if_name_ref);
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
    DebugLog("connection name = %s, len=%d\n", utf8_name, len);
    CFRelease(active_if_name_ref);
    CFRelease(names);
    return true;
  }
  return false;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  return true;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  if (!GetAuthorizationForRootPrivilege()) {
    return false;
  }
  return true;
}
