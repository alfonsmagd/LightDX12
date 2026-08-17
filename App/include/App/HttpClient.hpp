#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace App
{
	struct HttpResponse
	{
		uint32_t statusCode = 0;
		std::vector<uint8_t> body;

		std::string Text() const;
	};

	class HttpClient final
	{
	public:
		HttpClient( std::wstring host, uint16_t port, bool secure = false );

		HttpResponse Get( std::wstring_view path ) const;
		HttpResponse PostJson( std::wstring_view path, std::string_view json ) const;
		void DownloadToFile( std::wstring_view path, const std::wstring& destination ) const;

	private:
		HttpResponse Request( const wchar_t* verb, std::wstring_view path, std::string_view body, const wchar_t* contentType ) const;

		std::wstring host_;
		uint16_t port_ = 0;
		bool secure_ = false;
	};

	std::wstring Utf8ToWide( std::string_view text );
	std::string JsonEscape( std::string_view text );
	std::string JsonString( std::string_view json, std::string_view key );
}
