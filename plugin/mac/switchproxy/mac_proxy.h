//
//  mac_proxy.h
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//
#ifndef __MAC_PROXY_H__
#define __MAC_PROXY_H__
#include "proxy_config.h"

#include "proxy_base.h"

#include <Security/Security.h>
#include <SystemConfiguration/SystemConfiguration.h>

class MacProxy : public ProxyBase {
 public:
  MacProxy();
  ~MacProxy();
  virtual bool PlatformDependentStartup();
  virtual void PlatformDependentShutdown();
  virtual bool GetActiveConnectionName(const char** connection_name);
  virtual bool GetProxyConfig(ProxyConfig* config);
  virtual bool SetProxyConfig(const ProxyConfig& config);

 private:
  bool GetAuthorizationForRootPrivilege();
  
  static int NumberOfBytesInCFString(CFStringRef str);

  static bool GetBoolFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key,
      bool default_value);

  static int GetIntFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key,
      int default_value);

  static char* CopyUtf8StringFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key);

  static CFStringRef CreateCFStringFromUtf8String(char* utf8_str);

  static char* CreateUtf8StringFromString(CFStringRef str);

  static bool IsNetworkInterfaceActive(SCNetworkInterfaceRef net_if);

  static SCNetworkInterfaceRef GetActiveNetworkInterface(
      SCNetworkSetRef network_set);

  SCPreferencesRef sc_preference_;
  AuthorizationRef authorization_;;
};

#endif  // __MAC_PROXY_H__