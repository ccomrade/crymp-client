#include "Error.h"
#include "Format.h"
#include "WinAPI.h"

void Error::MakeWhat()
{
	if (m_code <= 0 && m_description.empty())
		m_what = m_message;
	else
		m_what = Format("%s: Error code %d (%s)", m_message.c_str(), m_code, m_description.c_str());
}

Error::Error(std::string && message)
{
	m_code = -1;
	m_message = std::move(message);

	MakeWhat();
}

Error::Error(std::string && message, int code, std::string && description)
{
	m_code = code;
	m_message = std::move(message);
	m_description = std::move(description);

	MakeWhat();
}

SystemError::SystemError(std::string && message, int code)
: Error(std::move(message), code, GetCodeDescription(code))
{
}

int SystemError::GetCurrentCode()
{
	return WinAPI::GetCurrentErrorCode();
}

std::string SystemError::GetCodeDescription(int code)
{
	return WinAPI::GetErrorCodeDescription(code);
}
