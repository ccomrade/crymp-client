#include "Library/Util.h"
#include "Library/WinAPI.h"

#include "HTTPClient.h"
#include "Client.h"
#include "Executor.h"

struct HTTPClientTask : public IExecutorTask
{
	HTTPClientRequest request;
	HTTPClientResult result;

	// worker thread
	void Execute() override
	{
		CryLog("%s %s", request.method.c_str(), request.url.c_str());
		try
		{
			result.code = WinAPI::HTTPRequest(
				request.method,
				request.url,
				request.data,
				request.headers,
				request.timeout,
				[this](uint64_t contentLength, const WinAPI::HTTPRequestReader & reader)
				{
					// content length is zero if not provided by the server
					result.response.reserve(contentLength);

					while (true)
					{
						char chunk[8192];
						const size_t chunkLength = reader(chunk, sizeof chunk);

						if (chunkLength == 0)
							break;

						result.response.append(chunk, chunkLength);
					}
				}
			);
		}
		catch (const Error & error)
		{
			result.error = error;
		}
	}

	// main thread
	void Callback() override
	{
		if (request.callback)
		{
			request.callback(result);
		}
	}
};

void HTTPClient::AddTelemetryHeaders(HTTPClientRequest & request)
{
	for (auto& master : gClient->GetMasters()) {
		if (Util::StartsWith(gClient->GetMasterServerAPI(master), request.url)) {
			request.headers["X-Sfwcl-HWID"] = m_hwid;
			request.headers["X-Sfwcl-Locale"] = m_locale;
			request.headers["X-Sfwcl-Tz"] = m_timezone;
			break;
		}
	}
}

HTTPClient::HTTPClient()
{
	m_hwid = gClient->GetHWID("idsvc");
	m_locale = WinAPI::GetLocale();
	m_timezone = std::to_string(WinAPI::GetTimeZoneBias());
}

HTTPClient::~HTTPClient()
{
}

void HTTPClient::Request(HTTPClientRequest && request)
{
	std::unique_ptr<HTTPClientTask> task = std::make_unique<HTTPClientTask>();
	task->request = std::move(request);

	AddTelemetryHeaders(task->request);

	gClient->GetExecutor()->AddTask(std::move(task));
}

void HTTPClient::GET(const std::string_view & url, std::function<void(HTTPClientResult&)> callback)
{
	std::unique_ptr<HTTPClientTask> task = std::make_unique<HTTPClientTask>();
	task->request.method = "GET";
	task->request.url = url;
	task->request.callback = std::move(callback);

	AddTelemetryHeaders(task->request);

	gClient->GetExecutor()->AddTask(std::move(task));
}
