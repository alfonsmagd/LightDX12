#include "App/MeshGenerationClient.hpp"

#include "App/HttpClient.hpp"
#include "App/TaskSystem.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace App
{
	MeshGenerationClient::MeshGenerationClient( TaskSystem& tasks, MeshApiConfig config ):
		tasks_( &tasks ), config_( std::move( config ) )
	{
	}

	bool MeshGenerationClient::Submit( std::string prompt )
	{
		if( prompt.empty() ) return false;
		bool expected = false;
		if( !busy_.compare_exchange_strong( expected, true ) ) return false;

		Publish( GenerationStage::Queued, {}, "Peticion en cola." );
		try
		{
			tasks_->Submit( "HTTP: generar malla", [this, prompt = std::move( prompt )]() mutable
			{
				try
				{
					RunRequest( std::move( prompt ) );
				}
				catch( const std::exception& exception )
				{
					Publish( GenerationStage::Failed, {}, exception.what() );
				}
				busy_.store( false );
			} );
		}
		catch( ... )
		{
			busy_.store( false );
			throw;
		}
		return true;
	}

	void MeshGenerationClient::RunRequest( std::string prompt )
	{
		HttpClient http( config_.host, config_.port );
		Publish( GenerationStage::Sending, {}, "Enviando prompt a Cube3D..." );
		const std::string body = "{\"prompt\":\"" + JsonEscape( prompt ) + "\"}";
		const HttpResponse create = http.PostJson( L"/v1/meshes", body );
		if( create.statusCode != 202 )
			throw std::runtime_error( "La API devolvio HTTP " + std::to_string( create.statusCode ) + ": " + create.Text() );

		const std::string jobId = JsonString( create.Text(), "job_id" );
		if( jobId.empty() ) throw std::runtime_error( "La API no devolvio job_id." );
		Publish( GenerationStage::Generating, jobId, "Cube3D esta generando la malla." );

		const std::wstring statusPath = L"/v1/meshes/" + Utf8ToWide( jobId );
		std::string assetUrl;
		for( ;; )
		{
			// The worker sleeps between status checks. It does not spin and the render
			// thread remains fully independent from network/model latency.
			std::this_thread::sleep_for( std::chrono::milliseconds( config_.pollIntervalMilliseconds ) );
			const HttpResponse statusResponse = http.Get( statusPath );
			if( statusResponse.statusCode != 200 )
				throw std::runtime_error( "Consulta de estado HTTP " + std::to_string( statusResponse.statusCode ) + "." );

			const std::string json = statusResponse.Text();
			const std::string status = JsonString( json, "status" );
			if( status == "completed" )
			{
				assetUrl = JsonString( json, "asset_url" );
				if( assetUrl.empty() ) assetUrl = "/v1/meshes/" + jobId + "/asset";
				break;
			}
			if( status == "failed" )
			{
				std::string error = JsonString( json, "error" );
				if( error.empty() ) error = "Cube3D no pudo generar la malla.";
				throw std::runtime_error( error );
			}
			if( status.empty() ) throw std::runtime_error( "Respuesta de estado no valida." );
		}

		Publish( GenerationStage::Downloading, jobId, "Descargando OBJ..." );
		const std::filesystem::path destination = config_.outputDirectory / ( jobId + ".obj" );
		http.DownloadToFile( Utf8ToWide( assetUrl ), destination.wstring() );
		if( !std::filesystem::is_regular_file( destination ) )
			throw std::runtime_error( "La descarga termino pero el OBJ no existe." );
		Publish( GenerationStage::Completed, jobId, "OBJ disponible para cargar.", destination );
	}

	void MeshGenerationClient::Publish( GenerationStage stage, std::string jobId, std::string message, std::filesystem::path path )
	{
		events_.Push( GenerationEvent{ stage, std::move( jobId ), std::move( message ), std::move( path ) } );
	}

	bool MeshGenerationClient::IsBusy() const noexcept { return busy_.load(); }
	std::optional<GenerationEvent> MeshGenerationClient::TryPopEvent() { return events_.TryPop(); }
	const MeshApiConfig& MeshGenerationClient::Config() const noexcept { return config_; }
}
