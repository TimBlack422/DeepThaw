#include "tools.h"

#include <wbemidl.h>
#include <comutil.h>

#pragma comment(lib,"comsuppw.lib")
#pragma comment(lib,"wbemuuid.lib")

//满足false的条件是：获取状态失败，vbs存在但未运行
bool IsVBSEnabled()
{
	
		auto hr =
			CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hr)) 
		{
			CoUninitialize();
			return false;
		}

		hr =
			CoInitializeSecurity
			(
				NULL,
				-1,
				NULL,
				NULL,
				RPC_C_AUTHN_LEVEL_DEFAULT,
				RPC_C_IMP_LEVEL_IMPERSONATE,
				NULL,
				EOAC_NONE,
				NULL
			);

		IWbemLocator* pLoc = nullptr;
		hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
		if (FAILED(hr))
		{
			CoUninitialize();
			return false;
		}

		IWbemServices* pServices = nullptr;
			hr = pLoc->ConnectServer(
				_bstr_t(L"ROOT\\Microsoft\\Windows\\DeviceGuard"),
				nullptr,
				nullptr,
				nullptr,
				0,
				nullptr,
				nullptr,
				&pServices
			);
			if(FAILED(hr))
			{
				pLoc->Release();

				CoUninitialize();
				return false;
			}

			hr = CoSetProxyBlanket(pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
			if (FAILED(hr)) {
				pServices->Release();
				pLoc->Release();

				CoUninitialize();
				return false;
			}

			//接下来是wmi查询
			IEnumWbemClassObject* pEnumerator = nullptr;
			hr = pServices->ExecQuery(
				_bstr_t(L"WQL"),
				_bstr_t(L"SELECT * FROM Win32_DeviceGuard"),
				WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
				nullptr,
				&pEnumerator
			);
			if(FAILED(hr))
			{
				pServices->Release();
				pLoc->Release();

				CoUninitialize();
				return false;
			}

			IWbemClassObject* pObject = nullptr;
			ULONG uReturned = 0;
			bool status = false;

			hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObject, &uReturned);
			if(SUCCEEDED(hr) && uReturned > 0 && pObject)
			{
				VARIANT vtProp;
				VariantInit(&vtProp);

				hr = pObject->Get(L"VirtualizationBasedSecurityStatus", 0, &vtProp, nullptr, nullptr);
				if (SUCCEEDED(hr) && vtProp.vt == VT_I4) {
					//statusCode_ = vtProp.lVal;
					status = (vtProp.lVal == 1) ? true : false;
				}
				VariantClear(&vtProp);

				pObject->Release();
			}
			else
			{
				pServices->Release();
				pLoc->Release();

				CoUninitialize();
				return false;
			}
			pEnumerator->Release();
			pServices->Release();
			pLoc->Release();

			CoUninitialize();
			return status;
	
}