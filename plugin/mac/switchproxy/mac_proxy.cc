//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

bool MacProxy::PlatformDependentStartup() {
  return TRUE;
}

void MacProxy::PlatformDependentShutdown() {

}

bool MacProxy::GetActiveConnectionName(char** connection_name) {
  char* name = (char*)malloc(10);
  strcpy(name, "Lan-Mac");
  *connection_name = name;
  return TRUE;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  return TRUE;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  return TRUE;
}
