#include <string>

namespace Interface_DeepFrz
{
	bool getDeepFrzStatus();
	bool getDeepFrzVersion(std::wstring & output_version);

	bool setDeepFrzStatus(bool Enable);
	bool deleteDeepFrz();
}