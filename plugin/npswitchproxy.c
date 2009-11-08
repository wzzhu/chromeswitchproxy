/* ***** BEGIN LICENSE BLOCK *****
* Copyright 2009 Wenzhang Zhu (wzzhu@cs.hku.hk)
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
* This code was based on the npsimple.c sample code in Gecko-sdk.
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
* ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <string.h>

#include <npapi.h>
#include <npupp.h>
#include <npruntime.h>
#include <windows.h>
#include <wininet.h>

#ifdef WIN32
typedef bool (__stdcall* InternetQueryOptionFunc) (
	HINTERNET hInternet, DWORD dwOption,
	LPVOID lpBuffer, LPDWORD lpdwBufferLength);

typedef bool (__stdcall* InternetSetOptionFunc) (
	HINTERNET hInternet, DWORD dwOption,
	LPVOID lpBuffer, DWORD dwBufferLength);

HINSTANCE hWinInetLib;
InternetQueryOptionFunc pInternetQueryOption;
InternetSetOptionFunc pInternetSetOption;

typedef enum proxyState {
	Proxy_Unknown = 0,
	Proxy_On,
	Proxy_Off
} ProxyState;

bool LoadWinInetDll() {
	hWinInetLib = LoadLibrary(TEXT("wininet.dll"));
	if (hWinInetLib == NULL) {
		return false;
	}
	pInternetQueryOption = (InternetQueryOptionFunc)GetProcAddress(
		HMODULE(hWinInetLib), "InternetQueryOptionA");
	if (pInternetQueryOption == NULL) {
		FreeLibrary(hWinInetLib);
		return false;
	}
	pInternetSetOption = (InternetSetOptionFunc)GetProcAddress(
		HMODULE(hWinInetLib), "InternetSetOptionA");
	if (pInternetSetOption == NULL) {
		FreeLibrary(hWinInetLib);
		return false;
	}
	return true;
}

void UnloadWinInetDll() {
	if (hWinInetLib != NULL) {
		FreeLibrary(hWinInetLib);
	}
}

ProxyState GetWinInetProxyState() {
	INTERNET_PER_CONN_OPTION options[1];
	options[0].dwOption = INTERNET_PER_CONN_FLAGS;
	INTERNET_PER_CONN_OPTION_LIST list;	
	unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);	
	list.dwSize = nSize;
	list.pszConnection = NULL;
	list.pOptions = options;
	list.dwOptionCount = 1;
	list.dwOptionError = 0;	
	if (pInternetQueryOption(
		NULL,
		INTERNET_OPTION_PER_CONNECTION_OPTION,
		&list,
		&nSize)) {
			DWORD proxyState = options[0].Value.dwValue;		
			if (proxyState & PROXY_TYPE_PROXY) {
				return Proxy_On;
			} else {
				return Proxy_Off;
			}
	}
	return Proxy_Unknown;
}

ProxyState ToggleWinInetProxyState() {
	INTERNET_PER_CONN_OPTION options[1];
	options[0].dwOption = INTERNET_PER_CONN_FLAGS;
	INTERNET_PER_CONN_OPTION_LIST list;	
	unsigned long nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);	
	list.dwSize = nSize;
	list.pszConnection = NULL;
	list.pOptions = options;
	list.dwOptionCount = 1;
	list.dwOptionError = 0;	
	if (pInternetQueryOption(
		NULL,
		INTERNET_OPTION_PER_CONNECTION_OPTION,
		&list,
		&nSize)) {
			DWORD dwProxyState = options[0].Value.dwValue;
			ProxyState state;
			if (dwProxyState & PROXY_TYPE_PROXY) {
				dwProxyState &= ~PROXY_TYPE_PROXY;
				state = Proxy_Off;
			} else {
				dwProxyState |= PROXY_TYPE_PROXY;
				state = Proxy_On;
			}
			options[0].Value.dwValue = dwProxyState;
			if (pInternetSetOption(
				NULL,
				INTERNET_OPTION_PER_CONNECTION_OPTION,
				&list,
				nSize)) {
					return state;
			}
	}
	return Proxy_Unknown;
}

#endif // WIN32

static NPObject *so              = NULL;
static NPNetscapeFuncs *npnfuncs = NULL;

/* NPN */

static void DebugLog(const char *msg) {
#ifdef DEBUG
#ifndef _WINDOWS
	fputs(msg, stderr);
#else
	FILE *out = fopen("\\npswitchproxy.log", "a");
	fputs(msg, out);
	fclose(out);
#endif
#endif
}

static bool
HasMethod(NPObject* obj, NPIdentifier methodName) {
	DebugLog("npswitchproxy: HasMethod\n");
	return true;
}

static bool
InvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result) {
	DebugLog("npswitchproxy: InvokeDefault\n");	
	return true;
}

static bool
InvokeGetProxyState(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result) {
	DebugLog("npswitchproxy: InvokeGetProxyState\n");
	ProxyState state = GetWinInetProxyState();
	result->type = NPVariantType_Int32;	
	if (state == Proxy_On) {
		result->value.intValue = 1;
	} else if (state == Proxy_Off) {
		result->value.intValue = 0;
	} else {
		result->value.intValue = -1;
	}
	return true;
}

static bool
InvokeToggleProxyState(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result) {
	DebugLog("npswitchproxy: InvokeToggleProxyState\n");
	ProxyState state = ToggleWinInetProxyState();
	result->type = NPVariantType_Int32;	
	if (state == Proxy_On) {
		result->value.intValue = 1;
	} else if (state == Proxy_Off) {
		result->value.intValue = 0;
	} else {
		result->value.intValue = -1;
	}
	return true;
}

static bool
Invoke(NPObject* obj, NPIdentifier methodName, const NPVariant *args, uint32_t argCount, NPVariant *result) {
	DebugLog("npswitchproxy: Invoke\n");
	char *name = npnfuncs->utf8fromidentifier(methodName);
	if(name && !strcmp((const char *)name, "getProxyState")) {
		return InvokeGetProxyState(obj, args, argCount, result);
	} else if (name && !strcmp((const char *)name, "toggleProxyState")) {
		return InvokeToggleProxyState(obj, args, argCount, result);
	} else {
		// aim exception handling
		npnfuncs->setexception(obj, "exception during invocation");
		return false;
	}
}

static bool
HasProperty(NPObject *obj, NPIdentifier propertyName) {
	DebugLog("npswitchproxy: HasProperty\n");
	return false;
}

static bool
GetProperty(NPObject *obj, NPIdentifier propertyName, NPVariant *result) {
	DebugLog("npswitchproxy: GetProperty\n");
	return false;
}

static NPClass npcRefObject = {
	NP_CLASS_STRUCT_VERSION,
	NULL,
	NULL,
	NULL,
	HasMethod,
	Invoke,
	InvokeDefault,
	HasProperty,
	GetProperty,
	NULL,
	NULL,
};

/* NPP */

static NPError
NewNPInstance(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char *argn[], char *argv[], NPSavedData *saved) {
	DebugLog("npswitchproxy: new\n");
	return NPERR_NO_ERROR;
}

static NPError
DestroyNPInstance(NPP instance, NPSavedData **save) {
	if(so)
		npnfuncs->releaseobject(so);
	so = NULL;
	DebugLog("npswitchproxy: DestroyNPInstance\n");
	return NPERR_NO_ERROR;
}

static NPError
GetValue(NPP instance, NPPVariable variable, void *value) {
	switch(variable) {
	default:
		DebugLog("npswitchproxy: GetValue - default\n");
		return NPERR_GENERIC_ERROR;
	case NPPVpluginNameString:
		DebugLog("npswitchproxy: GetValue - name string\n");
		*((char **)value) = "AplixFooPlugin";
		break;
	case NPPVpluginDescriptionString:
		DebugLog("npswitchproxy: GetValue - description string\n");
		*((char **)value) = "SwitchProxyPlugin plugin.";
		break;
	case NPPVpluginScriptableNPObject:
		DebugLog("npswitchproxy: GetValue - scriptable object\n");
		if(!so)
			so = npnfuncs->createobject(instance, &npcRefObject);
		npnfuncs->retainobject(so);
		*(NPObject **)value = so;
		break;
#if defined(XULRUNNER_SDK)
	case NPPVpluginNeedsXEmbed:
		DebugLog("npswitchproxy: GetValue - xembed\n");
		*((PRBool *)value) = PR_FALSE;
		break;
#endif
	}
	return NPERR_NO_ERROR;
}

static NPError /* expected by Safari on Darwin */
HandleEvent(NPP instance, void *ev) {
	DebugLog("npswitchproxy: HandleEvent\n");
	return NPERR_NO_ERROR;
}

static NPError /* expected by Opera */
SetWindow(NPP instance, NPWindow* pNPWindow) {
	DebugLog("npswitchproxy: SetWindow\n");
	return NPERR_NO_ERROR;
}

/* EXPORT */
#ifdef __cplusplus
extern "C" {
#endif

	NPError OSCALL
		NP_GetEntryPoints(NPPluginFuncs *nppfuncs) {
			DebugLog("npswitchproxy: NP_GetEntryPoints\n");
			nppfuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
			nppfuncs->newp          = NewNPInstance;
			nppfuncs->destroy       = DestroyNPInstance;
			nppfuncs->getvalue      = GetValue;
			nppfuncs->event         = HandleEvent;
			nppfuncs->setwindow     = SetWindow;

			return NPERR_NO_ERROR;
	}

#ifndef HIBYTE
#define HIBYTE(x) ((((uint32)(x)) & 0xff00) >> 8)
#endif

	NPError OSCALL
		NP_Initialize(NPNetscapeFuncs *npnf
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
		, NPPluginFuncs *nppfuncs)
#else
		)
#endif
	{
		DebugLog("npswitchproxy: NP_Initialize\n");
		if(npnf == NULL)
			return NPERR_INVALID_FUNCTABLE_ERROR;

		if(HIBYTE(npnf->version) > NP_VERSION_MAJOR)
			return NPERR_INCOMPATIBLE_VERSION_ERROR;

		npnfuncs = npnf;
#if !defined(_WINDOWS) && !defined(WEBKIT_DARWIN_SDK)
		NP_GetEntryPoints(nppfuncs);
#endif
#ifdef WIN32
		if (!LoadWinInetDll()) {
			return NPERR_MODULE_LOAD_FAILED_ERROR;
		}
#endif
		return NPERR_NO_ERROR;
	}

	NPError
		OSCALL NP_Shutdown() {
			DebugLog("npswitchproxy: NP_Shutdown\n");
#ifdef WIN32
			UnloadWinInetDll();
#endif
			return NPERR_NO_ERROR;
	}

	char *
		NP_GetMIMEDescription(void) {
			DebugLog("npswitchproxy: NP_GetMIMEDescription\n");
			return "application/x-switch-wininet-proxy::wzzhu@cs.hku.hk";
	}

	NPError OSCALL /* needs to be present for WebKit based browsers */
		NP_GetValue(void *npp, NPPVariable variable, void *value) {
			return GetValue((NPP)npp, variable, value);
	}

#ifdef __cplusplus
}
#endif
