//
//  mac_proxy.h
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "proxy_config.h"

bool PlatformDependentStartup();
void PlatformDependentShutdown();
bool GetActiveConnectionName(char** connection_name);
bool GetProxyConfig(ProxyConfig* config);
bool SetProxyConfig(const ProxyConfig& config);
