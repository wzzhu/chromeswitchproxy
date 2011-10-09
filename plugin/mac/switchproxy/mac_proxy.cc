//
//  mac_proxy.cc
//  chromeswitchproxy
//
//  Created by Wenzhang Zhu on 08/10/2011.
//  Copyright 2011 E-Intuit Limited. All rights reserved.
//

#include "mac_proxy.h"

MacProxy::MacProxy() : authorizationRef(NULL) {
}

MacProxy::~MacProxy() {
  AuthorizationFree(authorizationRef, kAuthorizationFlagDestroyRights);
}

Boolean MacProxy::GetAuthorizationForRootPrivilege() {
  OSStatus status;
  if (!authorizationRef) {
    status = AuthorizationCreate(NULL,
                                 kAuthorizationEmptyEnvironment,
                                 kAuthorizationFlagDefaults,
                                 &authorizationRef);
    if (status != errAuthorizationSuccess) {
      return FALSE;
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
    return FALSE;
  }
  return TRUE;
}

bool MacProxy::PlatformDependentStartup() {
  return TRUE;
}

void MacProxy::PlatformDependentShutdown() {

}

bool MacProxy::GetActiveConnectionName(char** connection_name) {
  return TRUE;
}

bool MacProxy::GetProxyConfig(ProxyConfig* config) {
  return TRUE;
}

bool MacProxy::SetProxyConfig(const ProxyConfig& config) {
  if (!GetAuthorizationForRootPrivilege()) {
    return FALSE;
  }
  return TRUE;
}
