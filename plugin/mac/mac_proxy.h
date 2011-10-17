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
  virtual bool GetActiveConnectionName(const void** connection_name);
  virtual bool GetProxyConfig(ProxyConfig* config);
  virtual bool SetProxyConfig(const ProxyConfig& config);

 private:
  bool GetAuthorizationForRootPrivilege();
  void ParseProxyLine(const char* start_proxy, const char* end_proxy,
                      char* proxy, char* port);

  void ParseProxyServerDescription(
      const char* proxy_server, char* proxies[4], char* ports[4]);

  OSStatus RunNetworkSetupCommand(char *const* args);

  static bool GetBoolFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key,
      bool default_value);

  static int GetIntFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key,
      int default_value);

  static char* CopyCStringFromDictionary(
      CFDictionaryRef dict,
      CFStringRef key);

  static char* CreateCStringFromString(CFStringRef str);

  static bool IsNetworkInterfaceActive(SCNetworkInterfaceRef net_if);

  static SCNetworkServiceRef CopyActiveNetworkService(
      SCNetworkSetRef network_set);

  AuthorizationRef authorization_;;
};

#endif  // __MAC_PROXY_H__