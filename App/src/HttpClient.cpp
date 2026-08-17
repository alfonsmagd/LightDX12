#include "App/HttpClient.hpp"

#include <windows.h>
#include <winhttp.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>

namespace App
{
	namespace
	{
		struct InternetHandle
		{
			HINTERNET value = nullptr;
			~InternetHandle() { if( value ) WinHttpCloseHandle( value ); }
		};

		[[noreturn]] void ThrowWinHttp( const char* operation )
		{
			throw std::runtime_error( std::string( operation ) + " failed (WinHTTP " + std::to_string( GetLastError() ) + ")." );
		}

	}

	std::string HttpResponse::Text() const
	{
		if( body.empty() ) return {};
		return std::string( reinterpret_cast<const char*>( body.data() ), body.size() );
	}

	HttpClient::HttpClient( std::wstring host, uint16_t port, bool secure ):
		host_( std::move( host ) ), port_( port ), secure_( secure )
	{
	}

	HttpResponse HttpClient::Get( std::wstring_view path ) const
	{
		return Request( L"GET", path, {}, nullptr );
	}

	HttpResponse HttpClient::PostJson( std::wstring_view path, std::string_view json ) const
	{
		return Request( L"POST", path, json, L"Content-Type: application/json; charset=utf-8\r\n" );
	}

	HttpResponse HttpClient::Request( const wchar_t* verb, std::wstring_view path, std::string_view body, const wchar_t* contentType ) const
	{
		InternetHandle session{ WinHttpOpen( L"LightDX12-AIMeshViewer/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 ) };
		if( !session.value ) ThrowWinHttp( "WinHttpOpen" );
		WinHttpSetTimeouts( session.value, 5000, 5000, 15000, 15000 );

		InternetHandle connection{ WinHttpConnect( session.value, host_.c_str(), port_, 0 ) };
		if( !connection.value ) ThrowWinHttp( "WinHttpConnect" );

		const std::wstring pathCopy( path );
		const DWORD flags = secure_ ? WINHTTP_FLAG_SECURE : 0;
		InternetHandle request{ WinHttpOpenRequest( connection.value, verb, pathCopy.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags ) };
		if( !request.value ) ThrowWinHttp( "WinHttpOpenRequest" );

		const void* bodyPointer = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
		if( !WinHttpSendRequest( request.value, contentType, contentType ? static_cast<DWORD>( -1L ) : 0, const_cast<void*>( bodyPointer ), static_cast<DWORD>( body.size() ), static_cast<DWORD>( body.size() ), 0 ) )
			ThrowWinHttp( "WinHttpSendRequest" );
		if( !WinHttpReceiveResponse( request.value, nullptr ) ) ThrowWinHttp( "WinHttpReceiveResponse" );

		HttpResponse response;
		DWORD statusSize = sizeof( response.statusCode );
		if( !WinHttpQueryHeaders( request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &response.statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX ) )
			ThrowWinHttp( "WinHttpQueryHeaders" );

		for( ;; )
		{
			DWORD available = 0;
			if( !WinHttpQueryDataAvailable( request.value, &available ) ) ThrowWinHttp( "WinHttpQueryDataAvailable" );
			if( available == 0 ) break;
			const size_t offset = response.body.size();
			response.body.resize( offset + available );
			DWORD read = 0;
			if( !WinHttpReadData( request.value, response.body.data() + offset, available, &read ) ) ThrowWinHttp( "WinHttpReadData" );
			response.body.resize( offset + read );
		}
		return response;
	}

	void HttpClient::DownloadToFile( std::wstring_view path, const std::wstring& destination ) const
	{
		const HttpResponse response = Get( path );
		if( response.statusCode != 200 )
			throw std::runtime_error( "Asset download returned HTTP " + std::to_string( response.statusCode ) + "." );
		const std::filesystem::path target( destination );
		std::filesystem::create_directories( target.parent_path() );
		const std::filesystem::path temporary = target.wstring() + L".part";
		{
			std::ofstream file( temporary, std::ios::binary | std::ios::trunc );
			if( !file ) throw std::runtime_error( "Cannot create downloaded OBJ file." );
			file.write( reinterpret_cast<const char*>( response.body.data() ), static_cast<std::streamsize>( response.body.size() ) );
		}
		std::error_code error;
		std::filesystem::rename( temporary, target, error );
		if( error )
		{
			std::filesystem::remove( target, error );
			error.clear();
			std::filesystem::rename( temporary, target, error );
		}
		if( error ) throw std::runtime_error( "Cannot finalize downloaded OBJ file." );
	}

	std::wstring Utf8ToWide( std::string_view text )
	{
		if( text.empty() ) return {};
		const int size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>( text.size() ), nullptr, 0 );
		if( size <= 0 ) throw std::runtime_error( "Invalid UTF-8 string." );
		std::wstring result( static_cast<size_t>( size ), L'\0' );
		MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>( text.size() ), result.data(), size );
		return result;
	}

	std::string JsonEscape( std::string_view text )
	{
		std::string result;
		result.reserve( text.size() + 16 );
		for( const unsigned char value : text )
		{
			switch( value )
			{
				case '\\': result += "\\\\"; break;
				case '"': result += "\\\""; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				default:
					if( value < 0x20 ) result += "?";
					else result.push_back( static_cast<char>( value ) );
			}
		}
		return result;
	}

	std::string JsonString( std::string_view json, std::string_view key )
	{
		const std::string marker = "\"" + std::string( key ) + "\"";
		size_t position = json.find( marker );
		if( position == std::string_view::npos ) return {};
		position = json.find( ':', position + marker.size() );
		if( position == std::string_view::npos ) return {};
		position = json.find( '"', position + 1 );
		if( position == std::string_view::npos ) return {};
		++position;
		std::string result;
		while( position < json.size() )
		{
			const char value = json[position++];
			if( value == '"' ) return result;
			if( value != '\\' || position >= json.size() )
			{
				result.push_back( value );
				continue;
			}
			const char escaped = json[position++];
			switch( escaped )
			{
				case '"': result.push_back( '"' ); break;
				case '\\': result.push_back( '\\' ); break;
				case 'n': result.push_back( '\n' ); break;
				case 'r': result.push_back( '\r' ); break;
				case 't': result.push_back( '\t' ); break;
				default: result.push_back( escaped ); break;
			}
		}
		return {};
	}
}
