//
//  proxy_base.h
//  switchproxy
//
//  Created by Wenzhang Zhu on 09/10/2011.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//
#ifndef __PROXY_BASE_H__
#define __PROXY_BASE_H__
#include "proxy_config.h"

class ProxyBase {
 public:
  virtual bool PlatformDependentStartup() { return TRUE; }
  virtual void PlatformDependentShutdown() {}
  virtual bool GetActiveConnectionName(const char** connection_name) = 0;
  virtual bool GetProxyConfig(ProxyConfig* config) = 0;
  virtual bool SetProxyConfig(const ProxyConfig& config) = 0; 
};
#endif //__PROXY_BASE_H__