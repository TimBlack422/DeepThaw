#pragma once
#include <Windows.h>
#include <string>

namespace Interface_RebootRestoreRx
{
	std::wstring GetConsoleContent();

	bool DisableRebootRestoreRxPasswordVerification();
	bool IsPasswordVerificationDisabled();

	bool GetStatus();
	bool UninstallRebootRestoreRx();
}

