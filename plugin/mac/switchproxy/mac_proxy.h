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

class MacProxy : public ProxyBase {
 public:
  virtual bool PlatformDependentStartup();
  virtual void PlatformDependentShutdown();
  virtual bool GetActiveConnectionName(char** connection_name);
  virtual bool GetProxyConfig(ProxyConfig* config);
  virtual bool SetProxyConfig(const ProxyConfig& config);
};

#endif  // __MAC_PROXY_H__