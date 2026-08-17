#include <string>

namespace Interface_DeepFrz
{
	bool getDeepFrzStatus();
	bool getDeepFrzVersion(std::wstring & output_version);

	bool setDeepFrzStatus(bool Enable);
	bool deleteDeepFrz();

	//Normal模式
	bool getDeepFrzStatusNormal();
	bool setDeepFrzStatusNormal(HWND hWnd,bool Enable);

	bool IsItsVersionWorkstation();
}
