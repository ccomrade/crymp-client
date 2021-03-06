#include "HTTP.h"

namespace
{
	char HexDigitToChar(int digit)
	{
		return (digit < 10) ? '0' + digit : 'A' + (digit - 10);
	}
}

const char *HTTP::StatusCodeToString(int code)
{
	switch (static_cast<EStatusCode>(code))
	{
		case HTTP::STATUS_OK:                    return "OK";
		case HTTP::STATUS_CREATED:               return "Created";
		case HTTP::STATUS_ACCEPTED:              return "Accepted";
		case HTTP::STATUS_NO_CONTENT:            return "No Content";

		case HTTP::STATUS_MOVED_PERMANENTLY:     return "Moved Permanently";
		case HTTP::STATUS_FOUND:                 return "Found";
		case HTTP::STATUS_SEE_OTHER:             return "See Other";
		case HTTP::STATUS_NOT_MODIFIED:          return "Not Modified";
		case HTTP::STATUS_TEMPORARY_REDIRECT:    return "Temporary Redirect";

		case HTTP::STATUS_BAD_REQUEST:           return "Bad Request";
		case HTTP::STATUS_UNAUTHORIZED:          return "Unauthorized";
		case HTTP::STATUS_FORBIDDEN:             return "Forbidden";
		case HTTP::STATUS_NOT_FOUND:             return "Not Found";

		case HTTP::STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
		case HTTP::STATUS_NOT_IMPLEMENTED:       return "Not Implemented";
		case HTTP::STATUS_BAD_GATEWAY:           return "Bad Gateway";
		case HTTP::STATUS_SERVICE_UNAVAILABLE:   return "Service Unavailable";
		case HTTP::STATUS_GATEWAY_TIMEOUT:       return "Gateway Timeout";
	}

	return "";
}

std::string HTTP::URLEncode(const std::string_view & text)
{
	std::string result;
	result.reserve(2 * text.length());

	for (char ch : text)
	{
		if ((ch >= 'a' && ch <= 'z')
		 || (ch >= 'A' && ch <= 'Z')
		 || (ch >= '0' && ch <= '9')
		 || ch == '.'
		 || ch == '-'
		 || ch == '_')
		{
			result += ch;
		}
		else
		{
			// percent-encoded character
			int code = static_cast<unsigned char>(ch);

			result += '%';
			result += HexDigitToChar(code >> 4);   // first digit
			result += HexDigitToChar(code & 0xF);  // second digit
		}
	}

	return result;
}
