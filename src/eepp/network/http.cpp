#include <eepp/network/http.hpp>
#include <eepp/network/ssl/sslsocket.hpp>
#include <eepp/network/uri.hpp>
#include <eepp/system/iostream.hpp>
#include <eepp/system/iostreamfile.hpp>
#include <eepp/system/iostreaminflate.hpp>
#include <eepp/system/iostreamstring.hpp>
#include <eepp/system/compression.hpp>
#include <cctype>
#include <algorithm>
#include <iterator>
#include <sstream>
#include <limits>

using namespace EE::Network::SSL;

namespace EE { namespace Network {

#define PACKET_BUFFER_SIZE (16384)

Http::Request::Method Http::Request::methodFromString( std::string methodString ) {
	String::toLowerInPlace(methodString);
	if ( "get" == methodString ) return Method::Get;
	else if ( "head" == methodString ) return Method::Head;
	else if ( "post" == methodString ) return Method::Post;
	else if ( "put" == methodString ) return Method::Put;
	else if ( "delete" == methodString ) return Method::Delete;
	else if ( "options" == methodString ) return Method::Options;
	else if ( "patch" == methodString ) return Method::Patch;
	else if ( "connect" == methodString ) return Method::Connect;
	else return Method::Get;
}

std::string Http::Request::methodToString(const Http::Request::Method& method) {
	switch (method) {
		default :
		case Get:		return "GET";
		case Head:		return "HEAD";
		case Post:		return "POST";
		case Put:		return "PUT";
		case Delete:	return "DELETE";
		case Options:	return "OPTIONS";
		case Patch:		return "PATCH";
		case Connect:	return "CONNECT";
	}
}

Http::Request::Request(const std::string& uri, Method method, const std::string& body, bool validateCertificate, bool validateHostname , bool followRedirect, bool compressedResponse) :
	mValidateCertificate( validateCertificate ),
	mValidateHostname( validateHostname ),
	mFollowRedirect( followRedirect ),
	mCompressedResponse( compressedResponse ),
	mCancel( false ),
	mMaxRedirections( 10 ),
	mRedirectionCount( 0 )
{
	setMethod(method);
	setUri(uri);
	setHttpVersion(1, 1);
	setBody(body);
}

void Http::Request::setField(const std::string& field, const std::string& value) {
	mFields[String::toLower(field)] = value;
}

void Http::Request::setMethod(Http::Request::Method method) {
	mMethod = method;
}

void Http::Request::setUri(const std::string& uri) {
	mUri = uri;

	// Make sure it starts with a '/'
	if (mUri.empty() || (mUri[0] != '/'))
		mUri.insert(0, "/");
}

void Http::Request::setHttpVersion(unsigned int major, unsigned int minor) {
	mMajorVersion = major;
	mMinorVersion = minor;
}

void Http::Request::setBody(const std::string& body) {
	mBody = body;
}

const std::string &Http::Request::getUri() const {
	return mUri;
}

const bool& Http::Request::getValidateCertificate() const {
	return mValidateCertificate;
}

void Http::Request::setValidateCertificate(bool enable) {
	mValidateCertificate = enable;
}

const bool &Http::Request::getValidateHostname() const {
	return mValidateHostname;
}

void Http::Request::setValidateHostname(bool enable) {
	mValidateHostname = enable;
}

const bool &Http::Request::getFollowRedirect() const {
	return mFollowRedirect;
}

void Http::Request::setFollowRedirect(bool follow) {
	mFollowRedirect = follow;
}

const unsigned int& Http::Request::getMaxRedirects() const {
	return mMaxRedirections;
}

void Http::Request::setMaxRedirects(unsigned int maxRedirects) {
	mMaxRedirections = maxRedirects;
}

void Http::Request::setProgressCallback(const Http::Request::ProgressCallback& progressCallback) {
	mProgressCallback = progressCallback;
}

const Http::Request::ProgressCallback& Http::Request::getProgressCallback() const {
	return mProgressCallback;
}

void Http::Request::cancel() {
	mCancel = true;
}

const bool &Http::Request::isCancelled() const {
	return mCancel;
}

std::string Http::Request::prepareTunnel(const Http& http) {
	std::ostringstream out;

	setMethod( Connect );

	std::string method = methodToString( mMethod );

	out << method << " " << http.getHostName() << ":" << http.getPort() << " ";
	out << "HTTP/" << mMajorVersion << "." << mMinorVersion << "\r\n";

	setField( "Host", String::format( "%s:%d", http.getHostName().c_str(), http.getPort() ) );
	setField( "Proxy-Connection", "Keep-Alive" );
	setField( "User-Agent", "eepp-network" );

	for (FieldTable::const_iterator i = mFields.begin(); i != mFields.end(); ++i)
		out << i->first << ": " << i->second << "\r\n";

	out << "\r\n";

	return out.str();
}

const bool& Http::Request::isCompressedResponse() const {
	return mCompressedResponse;
}

void Http::Request::setCompressedResponse(const bool& compressedResponse) {
	mCompressedResponse = compressedResponse;
}

std::string Http::Request::prepare(const Http& http) const {
	std::ostringstream out;

	// Convert the method to its string representation
	std::string method = methodToString( mMethod );

	// Write the first line containing the request type
	if ( http.getProxy().empty() ) {
		out << method << " " << mUri << " ";
	} else {
		URI uri = http.getURI();
		uri.setPathEtc( mUri );
		out << method << " " << uri.toString() << " ";
	}

	out << "HTTP/" << mMajorVersion << "." << mMinorVersion << "\r\n";

	// Write fields
	for (FieldTable::const_iterator i = mFields.begin(); i != mFields.end(); ++i) {
		out << i->first << ": " << i->second << "\r\n";
	}

	// Use an extra \r\n to separate the header from the body
	out << "\r\n";

	// Add the body
	out << mBody;

	return out.str();
}

bool Http::Request::hasField(const std::string& field) const {
	return mFields.find(String::toLower(field)) != mFields.end();
}

const std::string& Http::Request::getField(const std::string& field) const {
	FieldTable::const_iterator it = mFields.find(String::toLower(field));
	if (it != mFields.end()) {
		return it->second;
	} else {
		static const std::string empty = "";
		return empty;
	}
}

const char * Http::Response::statusToString( const Http::Response::Status& status ) {
	switch ( status ) {
		// 2xx: success
		case Ok: return "OK";
		case Created: return "Created";
		case Accepted: return "Accepted";
		case NoContent: return "No Content";
		case ResetContent: return "Reset Content";
		case PartialContent: return "Partial Content";

		// 3xx: redirection
		case MultipleChoices: return "Multiple Choices";
		case MovedPermanently: return "Moved Permanently";
		case MovedTemporarily: return "Moved Temporarily";
		case NotModified: return "Not Modified";

		// 4xx: client error
		case BadRequest: return "BadRequest";
		case Unauthorized: return "Unauthorized";
		case Forbidden: return "Forbidden";
		case NotFound: return "Not Found";
		case RangeNotSatisfiable: return "Range Not Satisfiable";

		// 5xx: server error
		case InternalServerError: return "Internal Server Error";
		case NotImplemented: return "Not Implemented";
		case BadGateway: return "Bad Gateway";
		case ServiceNotAvailable: return "Service Not Available";
		case GatewayTimeout: return "Gateway Timeout";
		case VersionNotSupported: return "Version Not Supported";

		// 10xx: Custom codes
		case InvalidResponse: return "Invalid Response";
		case ConnectionFailed: return "Connection Failed";
		default: return "";
	}
}

Http::Response::Response() :
	mStatus	  (ConnectionFailed),
	mMajorVersion(0),
	mMinorVersion(0)
{
}

Http::Response::FieldTable Http::Response::getHeaders() {
	return mFields;
}

const std::string& Http::Response::getField(const std::string& field) const {
	FieldTable::const_iterator it = mFields.find(String::toLower(field));
	if (it != mFields.end()) {
		return it->second;
	} else {
		static const std::string empty = "";
		return empty;
	}
}

Http::Response::Status Http::Response::getStatus() const {
	return mStatus;
}

const char * Http::Response::getStatusDescription() const {
	switch ( mStatus ) {
		// 2xx: success
		case Ok: return "Successfull";
		case Created: return "The resource has successfully been created";
		case Accepted: return "The request has been accepted, but will be processed later by the server";
		case NoContent: return "The server didn't send any data in return";
		case ResetContent: return "The server informs the client that it should clear the view (form) that caused the request to be sent";
		case PartialContent: return "The server has sent a part of the resource, as a response to a partial GET request";

		// 3xx: redirection
		case MultipleChoices: return "The requested page can be accessed from several locations";
		case MovedPermanently: return "The requested page has permanently moved to a new location";
		case MovedTemporarily: return "The requested page has temporarily moved to a new location";
		case NotModified: return "For conditionnal requests, means the requested page hasn't changed and doesn't need to be refreshed";

		// 4xx: client error
		case BadRequest: return "The server couldn't understand the request (syntax error)";
		case Unauthorized: return "The requested page needs an authentification to be accessed";
		case Forbidden: return "The requested page cannot be accessed at all, even with authentification";
		case NotFound: return "The requested page doesn't exist";
		case RangeNotSatisfiable: return "The server can't satisfy the partial GET request (with a \"Range\" header field)";

		// 5xx: server error
		case InternalServerError: return "The server encountered an unexpected error";
		case NotImplemented: return "The server doesn't implement a requested feature";
		case BadGateway: return "The gateway server has received an error from the source server";
		case ServiceNotAvailable: return "The server is temporarily unavailable (overloaded, in maintenance, ...)";
		case GatewayTimeout: return "The gateway server couldn't receive a response from the source server";
		case VersionNotSupported: return "The server doesn't support the requested HTTP version";

		// 10xx: Custom codes
		case InvalidResponse: return "Response is not a valid HTTP one";
		case ConnectionFailed: return "Connection with server failed";
		default: return "Unknown response status";
	}
}

unsigned int Http::Response::getMajorHttpVersion() const {
	return mMajorVersion;
}

unsigned int Http::Response::getMinorHttpVersion() const {
	return mMinorVersion;
}

const std::string& Http::Response::getBody() const {
	return mBody;
}

void Http::Response::parse(const std::string& data) {
	std::istringstream in(data);

	// Extract the HTTP version from the first line
	std::string version;

	if (in >> version) {
		std::locale loc;
		if ((version.size() >= 8) && (version[6] == '.') &&
			(String::toLower(version.substr(0, 5)) == "http/")   &&
			 std::isdigit(version[5],loc) && std::isdigit(version[7],loc)) {
			mMajorVersion = version[5] - '0';
			mMinorVersion = version[7] - '0';
		} else {
			// Invalid HTTP version
			mStatus = InvalidResponse;
			return;
		}
	}

	// Extract the status code from the first line
	int status = InvalidResponse;

	if (in >> status) {
		mStatus = static_cast<Status>(status);
	} else {
		// Invalid status code
		mStatus = InvalidResponse;
		return;
	}

	// Ignore the end of the first line
	in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	// Parse the other lines, which contain fields, one by one
	parseFields(in);

	mBody.clear();
}

void Http::Response::parseFields(std::istream &in) {
	std::string line;
	while (std::getline(in, line) && (line.size() > 2)) {
		std::string::size_type pos = line.find(": ");

		if (pos != std::string::npos) {
			// Extract the field name and its value
			std::string field = line.substr(0, pos);
			std::string value = line.substr(pos + 2);

			// Remove any trailing \r
			if (!value.empty() && (*value.rbegin() == '\r'))
				value.erase(value.size() - 1);

			// Add the field
			mFields[String::toLower(field)] = value;
		}
	}
}

Http::Http() :
	mConnection( NULL ),
	mHost(),
	mPort(0),
	mIsSSL( false )
{
}

Http::Http(const std::string & host, unsigned short port, bool useSSL, URI proxy) :
	mConnection( NULL ),
	mHostName(host),
	mPort(port),
	mIsSSL( useSSL ),
	mProxy(proxy)
{
	setHost(host, port, useSSL, proxy);
}

Http::~Http() {
	std::list<AsyncRequest*>::iterator itt;

	// First we wait to finish any request pending
	for ( auto&& itt : mThreads ) {
		itt->wait();
	}

	for ( auto&& itt : mThreads ) {
		eeDelete( itt );
	}

	// Then we destroy the last open connection
	HttpConnection * connection = mConnection;

	eeSAFE_DELETE( connection );
}

void Http::setHost(const std::string& host, unsigned short port, bool useSSL, URI proxy) {
	mProxy = proxy;

	bool sameHost( host == mHostName && port == mPort && useSSL == mIsSSL );

	// Check the protocol
	if (String::toLower(host.substr(0, 7)) == "http://") {
		// HTTP protocol
		mHostName = host.substr(7);
		mPort	 = (port != 0 ? port : 80);
	} else if (String::toLower(host.substr(0, 8)) == "https://") {
		// HTTPS protocol
		#ifdef EE_SSL_SUPPORT
		mIsSSL		= true;
		mHostName	= host.substr(8);
		mPort		= (port != 0 ? port : 443);
		#else
		mHostName	= "";
		mPort		= 0;
		#endif
	} else {
		// Undefined protocol - use HTTP, unless SSL is specified
		mHostName	= host;
		mPort		= (port != 0 ? port : 80);

		#ifdef EE_SSL_SUPPORT
		mPort		= useSSL ? (port != 0 ? port : 443) : mPort;
		mIsSSL		= useSSL || mPort == 443;
		#endif
	}

	// Remove any trailing '/' from the host name
	if (!mHostName.empty() && (*mHostName.rbegin() == '/'))
		mHostName.erase(mHostName.size() - 1);

	if ( !mProxy.empty() ) {
		mHost = IpAddress(mProxy.getHost());
		sameHost = false;
	} else {
		mHost = IpAddress(mHostName);
	}

	// If the new host is different to the last set host
	// and there's an open connection to the host, we close
	// the old connection to prepare a new one.
	if ( !sameHost && NULL != mConnection ) {
		HttpConnection * connection = mConnection;
		eeSAFE_DELETE( connection );
		mConnection = NULL;
	}
}

Http::Response Http::sendRequest(const Http::Request& request, Time timeout) {
	IOStreamString stream;
	Response response = downloadRequest( request, stream, timeout );
	response.mBody = stream.getStream();
	return response;
}

Http::Response Http::downloadRequest(const Http::Request& request, IOStream& writeTo, Time timeout) {
	if ( 0 == mHost.toInteger() ) {
		return Response();
	}

	if ( NULL == mConnection ) {
		HttpConnection * connection = eeNew( HttpConnection, () );
		TcpSocket * socket = NULL;

		// If the http client is proxied and the end host use SSL
		// We need to create an HTTP Tunnel against the proxy server
		if ( isProxied() && mIsSSL && SSLSocket::isSupported() ) {
			socket	= SSLSocket::New( mHostName, request.getValidateCertificate(), request.getValidateHostname() );

			connection->setSSL( true );
		} else {
			bool isSSL = !isProxied() ? mIsSSL : ( SSLSocket::isSupported() && mProxy.getScheme() == "https" );

			socket	= isSSL ? SSLSocket::New( mHostName, request.getValidateCertificate(), request.getValidateHostname() ) :
							  TcpSocket::New();

			connection->setSSL( isSSL );
		}

		connection->setSocket( socket );

		mConnection			= connection;
	}

	// First make sure that the request is valid -- add missing mandatory fields
	Request toSend(prepareFields(request));

	// Prepare the response
	Response received;

	// If not connected, try to connect to the server
	if ( !mConnection->isConnected() ) {
		// We need to create an HTTP Tunnel?
		if ( isProxied() && mIsSSL && SSLSocket::isSupported() ) {
			SSLSocket * sslSocket = reinterpret_cast<SSLSocket*>( mConnection->getSocket() );

			// For an HTTP Tunnel first we need to connect to the proxy server ( without TLS )
			if (sslSocket->tcpConnect( mHost, mProxy.getPort(), timeout ) != Socket::Done) {
				return received;
			} else {
				mConnection->setConnected(true);
			}
		} else {
			if (mConnection->getSocket()->connect(mHost, mProxy.empty() ? mPort : mProxy.getPort(), timeout) != Socket::Done) {
				return received;
			} else {
				mConnection->setConnected(true);
			}
		}
	}

	// Connect the socket to the host
	if (mConnection->isConnected()) {
		// Create a HTTP Tunnel for SSL connections if not ready
		if ( isProxied() && mIsSSL && !mConnection->isTunneled() ) {
			// Create the HTTP Tunnel request
			Request tunnelRequest;
			std::string tunnelStr = tunnelRequest.prepareTunnel(*this);

			SSLSocket * sslSocket = reinterpret_cast<SSLSocket*>( mConnection->getSocket() );
			std::size_t sent;

			// Send the request
			if (sslSocket->tcpSend(tunnelStr.c_str(), tunnelStr.size(), sent) == Socket::Done) {
				char buffer[PACKET_BUFFER_SIZE+1];
				std::size_t readed = 0;

				// Get the proxy server response
				if (sslSocket->tcpReceive(buffer, PACKET_BUFFER_SIZE, readed) == Socket::Done) {
					// Parse the HTTP Tunnel request response
					Response tunnelResponse;
					std::string header;
					header.append( buffer, readed );
					tunnelResponse.parse(header);

					if ( tunnelResponse.getStatus() == Response::Ok ) {
						// Stablish the SSL connection if the response is positive
						if (sslSocket->sslConnect( mHost, mProxy.getPort(), timeout ) != Socket::Done) {
							return received;
						}
					} else {
						return tunnelResponse;
					}
				} else {
					return received;
				}

				mConnection->setTunneled(true);
				mConnection->setKeepAlive(true);
			}
		}

		// Convert the request to string and send it through the connected socket
		std::string requestStr = toSend.prepare(*this);

		if (!requestStr.empty()) {
			Socket::Status status;

			// Send it through the socket
			if (mConnection->getSocket()->send(requestStr.c_str(), requestStr.size()) == Socket::Done) {
				// Wait for the server's response
				std::size_t currentTotalBytes = 0;
				std::size_t len = 0;
				std::size_t readed = 0;
				char * eol; // end of line
				char * bol; // beginning of line
				char buffer[PACKET_BUFFER_SIZE+1];
				std::string headerBuffer;
				std::string chunkBuffer;
				IOStreamString fileBuffer;
				bool isnheader = false;
				bool chunked = false;
				bool chunkNewBuffer = false;
				bool chunkEnded = false;
				bool compressed = false;
				IOStreamInflate * inflateStream = NULL;
				ios_size inflateChunkSize = 0;
				std::size_t contentLength = 0;

				while (!request.isCancelled() && ( status = mConnection->getSocket()->receive(buffer, PACKET_BUFFER_SIZE, readed) ) == Socket::Done) {
					// If we didn't receive the header yet, we will try to find the end of the header
					if ( !isnheader ) {
						// calculate combined length of unprocessed data and new data
						len += readed;

						// NULL terminate buffer for string functions
						buffer[len] = '\0';

						// checks if the header break happened to be the first line of the buffer
						if ( 0 == strncmp( buffer, "\r\n", 2 ) ) {
							if (len > 2) {
								currentTotalBytes += (len-2);
								chunkBuffer.append(buffer, buffer + (len-2));
							}

							continue;
						}

						if ( 0 == strncmp( buffer, "\n", 1 ) ) {
							if ( len > 1 ) {
								currentTotalBytes += (len-1);
								chunkBuffer.append(buffer, buffer + (len-1));
							}

							continue;
						}

						// process each line in buffer looking for header break
						bol = buffer;

						while( !isnheader && ( eol = strchr( bol, '\n') ) != NULL ) {
							// update bol based upon the value of eol
							bol = eol + 1;

							// test if end of headers has been reached
							if ( 0 == strncmp( bol, "\r\n", 2 ) || 0 == strncmp( bol, "\n", 1) ) {
								// note that end of headers has been reached
								isnheader = true;

								// update the value of bol to reflect the beginning of the line
								// immediately after the headers
								if ( bol[0] != '\n' )
									bol += 1;

								bol += 1;

								// calculate the amount of data remaining in the buffer
								len = readed - ( bol - buffer );

								// write remaining data to FILE stream
								if ( len > 0 ) {
									currentTotalBytes += len;
									chunkBuffer.append(bol, bol + len);
								}

								headerBuffer.append( buffer, ( bol - buffer ) );

								// reset length of left over data to zero and continue processing
								// non-header information
								len = 0;

								if ( !headerBuffer.empty() ) {
									// Build the Response object from the received data
									received.parse(headerBuffer);

									headerBuffer.clear();

									// Check if the response is chunked
									chunked = received.getField("transfer-encoding") == "chunked";

									// Check if the content is compressed
									std::string encoding( received.getField("content-encoding") );
									compressed = encoding == "gzip" || encoding == "deflate";

									if ( compressed ) {
										Compression::Mode compressionMode = "gzip" == encoding ? Compression::MODE_GZIP : Compression::MODE_DEFLATE;

										inflateChunkSize = Compression::getModeDefaultChunkSize( compressionMode );

										inflateStream = IOStreamInflate::New( writeTo, compressionMode );
									}

									// Get the content length
									if ( !received.getField("content-length").empty() ) {
										if ( !String::fromString( contentLength, received.getField("content-length") ) )
											contentLength = 0;
									}

									if ( received.getField("connection") == "closed" ) {
										mConnection->setConnected(false);
										mConnection->setTunneled(false);
									}

									// If a redirection is requested, and requests follows redirections,
									// send a new request to the redirection location.
									if ( ( received.getStatus() == Response::MovedPermanently || received.getStatus() == Response::MovedTemporarily ) &&
										 request.getFollowRedirect() ) {

										// Only continue redirecting if less than 10 redirections were done
										if ( request.mRedirectionCount < request.getMaxRedirects() ) {
											std::string location( received.getField("location") );
											URI uri( location );
											Http http( uri.getHost(), uri.getPort(), uri.getScheme() == "https" ? true : false );
											Http::Request newRequest( request );
											newRequest.setUri( uri.getPathEtc() );

											// Close the connection
											if ( !mConnection->isKeepAlive() )
												mConnection->disconnect();

											request.mRedirectionCount++;

											return http.downloadRequest( request, writeTo, timeout );
										}
									}

									// If is not chunked just save the file buffer and clear it
									if ( !chunked && !chunkBuffer.empty() ) {
										fileBuffer.write( &chunkBuffer[0], chunkBuffer.size() );
										chunkBuffer.clear();
									}

									if ( chunked )
										readed = 0;
								}
							}
						}

						if ( !isnheader ) {
							headerBuffer.append( buffer, ( bol - buffer ) );
						}
					}

					if ( isnheader ) {
						currentTotalBytes += readed;

						if ( chunked && readed ) {
							// If the chunk reading ended we just add the buffer received as a header
							// Otherwise we process the buffer data as chunk
							if ( !chunkEnded ) {
								// Keep a chunk buffer until the end of chunk is found
								chunkBuffer.append( buffer, buffer + readed );

								// If the new chunk starts with \r\n and the last removed chunk
								// did not contain the trailing \r\n, we remove it to detect
								// correctly the next length data
								if ( chunkNewBuffer ) {
									if ( chunkBuffer.substr( 0, 2 ) == "\r\n" ) {
										chunkBuffer = chunkBuffer.substr( 2 );
									}

									chunkNewBuffer = false;
								}

								// Check for the first \r\n to find the end of the length definition
								std::string::size_type lenEnd = chunkBuffer.find_first_of("\r\n");

								if ( lenEnd != std::string::npos ) {
									std::string::size_type firstCharPos = lenEnd + 2;
									unsigned long length;

									// Get the length of the chunk
									bool res = String::fromString( length, chunkBuffer.substr(0, lenEnd), std::hex );

									// If the length is solved...
									if ( res ) {
										// And it's bigger than 0, means that there are more chunks
										if ( length > 0 ) {
											// Check if the chunk buffer size at least equals to the length reported
											if ( chunkBuffer.size() - firstCharPos >= length ) {
												// In that case write the chunk to the file buffer
												fileBuffer.write( &chunkBuffer[firstCharPos], length );

												// And keep the remaining not completed chunk
												chunkBuffer = chunkBuffer.substr( firstCharPos + length );
												chunkNewBuffer = true;

												// Check if already have the \r\n of the next length in the buffer
												if ( !chunkBuffer.empty() && chunkBuffer.substr( 0, 2 ) == "\r\n" ) {
													// Remove it to be able to read the next length
													chunkBuffer = chunkBuffer.substr( 2 );
													chunkNewBuffer = false;
												}
											}
										} else {
											// If the value is 0 means that the data ended
											// But after this we can receive extra headers
											chunkEnded = true;
										}
									}
								}
							} else {
								headerBuffer.append( buffer, buffer + readed );
							}
						} else {
							// If not chunked just write into the file buffer
							fileBuffer.write( buffer, readed );
						}

						if ( compressed ) {
							if ( fileBuffer.getSize() - inflateChunkSize >= 0 ) {
								inflateStream->write( fileBuffer.getStreamPointer(), inflateChunkSize );

								IOStreamString newFileBuffer;

								fileBuffer.seek(inflateChunkSize);

								std::size_t trailing = fileBuffer.getSize() - inflateChunkSize;

								if ( trailing > 0 )
									newFileBuffer.write( fileBuffer.getPositionPointer(), trailing );

								fileBuffer = newFileBuffer;
							}
						} else {
							fileBuffer.seek(0);
							writeTo.write( fileBuffer.getPositionPointer(), fileBuffer.getSize() );
							fileBuffer.clear();
						}

						if ( request.getProgressCallback() ) {
							if ( !request.getProgressCallback()( *this, request, contentLength, currentTotalBytes ) ) {
								request.mCancel = true;
								break;
							}
						}
					}
				}

				if ( !headerBuffer.empty() ) {
					std::istringstream in(headerBuffer);
					received.parseFields(in);
				}

				if ( compressed && fileBuffer.getSize() > 0 ) {
					inflateStream->write( fileBuffer.getStreamPointer(), fileBuffer.getSize() );
				}

				if ( status == Socket::Status::Disconnected ) {
					mConnection->setConnected(false);
					mConnection->setTunneled(false);
				}

				eeSAFE_DELETE( inflateStream );
			} else {
				mConnection->setConnected(false);
				mConnection->setTunneled(false);
			}
		}

		// Close the connection
		if ( !mConnection->isKeepAlive() )
			mConnection->disconnect();
	}

	return received;
}

Http::Response Http::downloadRequest(const Http::Request & request, std::string writePath, Time timeout) {
	IOStreamFile file( writePath, "wb+" );
	return downloadRequest( request, file, timeout );
}

Http::AsyncRequest::AsyncRequest(Http *http, AsyncResponseCallback cb, Http::Request request, Time timeout) :
	mHttp( http ),
	mCb( cb ),
	mRequest( request ),
	mTimeout( timeout ),
	mRunning( true ),
	mStreamed( false ),
	mStreamOwned( false ),
	mStream(NULL)
{
}

Http::AsyncRequest::AsyncRequest(Http * http, Http::AsyncResponseCallback cb, Http::Request request, IOStream & writeTo, Time timeout) :
	mHttp( http ),
	mCb( cb ),
	mRequest( request ),
	mTimeout( timeout ),
	mRunning( true ),
	mStreamed( true ),
	mStreamOwned( false ),
	mStream( &writeTo )
{
}

Http::AsyncRequest::AsyncRequest(Http * http, Http::AsyncResponseCallback cb, Http::Request request, std::string writePath, Time timeout) :
	mHttp( http ),
	mCb( cb ),
	mRequest( request ),
	mTimeout( timeout ),
	mRunning( true ),
	mStreamed( true ),
	mStreamOwned( true ),
	mStream( IOStreamFile::New( writePath, "wb" ) )
{
}

Http::AsyncRequest::~AsyncRequest() {
	if ( mStreamOwned )
		eeSAFE_DELETE( mStream );
}

void Http::AsyncRequest::run() {
	Http::Response response = mStreamed ? mHttp->downloadRequest( mRequest, *mStream, mTimeout ) : mHttp->sendRequest( mRequest, mTimeout );

	mCb( *mHttp, mRequest, response );

	if ( mStreamed && mStreamOwned ) {
		eeSAFE_DELETE( mStream );
	}

	// The Async Request destroys the socket used to create the request
	HttpConnection * connection = mHttp->mConnection;
	eeSAFE_DELETE( connection );
	mHttp->mConnection = NULL;

	mRunning = false;
}

void Http::removeOldThreads() {
	std::list<AsyncRequest*> remove;

	std::list<AsyncRequest*>::iterator it = mThreads.begin();

	for ( ; it != mThreads.end(); ++it ) {
		AsyncRequest * ar = (*it);

		if ( !ar->mRunning ) {
			// We need to be sure, since the state is set in the thread, this will not block the thread anyway
			ar->wait();

			eeDelete( ar );

			remove.push_back( ar );
		}
	}

	for ( it = remove.begin(); it != remove.end(); ++it ) {
		mThreads.remove( (*it) );
	}
}

Http::Request Http::prepareFields(const Http::Request& request) {
	Request toSend(request);

	if (!toSend.hasField("User-Agent"))
		toSend.setField("User-Agent", "eepp-network");

	if (!toSend.hasField("Host"))
		toSend.setField("Host", mHostName);

	if (!toSend.hasField("Content-Length")) {
		std::ostringstream out;
		out << toSend.mBody.size();
		toSend.setField("Content-Length", out.str());
	}

	if ((toSend.mMethod == Request::Post) && !toSend.hasField("Content-Type"))
		toSend.setField("Content-Type", "application/x-www-form-urlencoded");

	if ((toSend.mMajorVersion * 10 + toSend.mMinorVersion >= 11) && !toSend.hasField("Connection"))
		toSend.setField("Connection", "close");

	if (!mProxy.empty()) {
		toSend.setField("Accept", "*/*");

		if ( mIsSSL ) {
			toSend.setField("Proxy-connection", "keep-alive");
		} else {
			toSend.setField("Proxy-connection", "close");
		}
	}

	if ( request.isCompressedResponse() )
		toSend.setField("Accept-Encoding", "gzip, deflate");

	return toSend;
}

void Http::setProxy(const URI& uri) {
	setHost( mHostName, mPort, mIsSSL, uri );
}

const URI& Http::getProxy() const {
	return mProxy;
}

bool Http::isProxied() const {
	return !mProxy.empty();
}

void Http::sendAsyncRequest( AsyncResponseCallback cb, const Http::Request& request, Time timeout ) {
	AsyncRequest * thread = eeNew( AsyncRequest, ( this, cb, request, timeout ) );

	thread->launch();

	// Clean old threads
	Lock l( mThreadsMutex );

	removeOldThreads();

	mThreads.push_back( thread );
}

void Http::downloadAsyncRequest(Http::AsyncResponseCallback cb, const Http::Request& request, IOStream& writeTo, Time timeout) {
	AsyncRequest * thread = eeNew( AsyncRequest, ( this, cb, request, writeTo, timeout ) );

	thread->launch();

	// Clean old threads
	Lock l( mThreadsMutex );

	removeOldThreads();

	mThreads.push_back( thread );
}

void Http::downloadAsyncRequest(Http::AsyncResponseCallback cb, const Http::Request& request, std::string writePath, Time timeout) {
	AsyncRequest * thread = eeNew( AsyncRequest, ( this, cb, request, writePath, timeout ) );

	thread->launch();

	// Clean old threads
	Lock l( mThreadsMutex );

	removeOldThreads();

	mThreads.push_back( thread );
}

const IpAddress& Http::getHost() const {
	return mHost;
}

const std::string& Http::getHostName() const {
	return mHostName;
}

const unsigned short& Http::getPort() const {
	return mPort;
}

const bool& Http::isSSL() const {
	return mIsSSL;
}

URI Http::getURI() const {
	return URI( String::format( "%s://%s:%d", mIsSSL ? "https" : "http", mHostName.c_str(), mPort ) );
}

Http::HttpConnection::HttpConnection() :
	mSocket(NULL),
	mIsConnected(false),
	mIsTunneled(false),
	mIsSSL(false),
	mIsKeepAlive(false)
{}

Http::HttpConnection::HttpConnection(TcpSocket * socket) :
	mSocket( socket ),
	mIsConnected( false ),
	mIsTunneled( false ),
	mIsSSL( false )
{}

Http::HttpConnection::~HttpConnection() {
	eeSAFE_DELETE(mSocket);
}

void Http::HttpConnection::setSocket(TcpSocket * socket) {
	mSocket = socket;
}

TcpSocket *Http::HttpConnection::getSocket() const {
	return mSocket;
}

void Http::HttpConnection::disconnect() {
	if ( NULL != mSocket )
		mSocket->disconnect();

	mIsConnected = false;
}

const bool &Http::HttpConnection::isConnected() const {
	return mIsConnected;
}

void Http::HttpConnection::setConnected(const bool & connected) {
	mIsConnected = connected;
}

const bool &Http::HttpConnection::isTunneled() const {
	return mIsTunneled;
}

void Http::HttpConnection::setTunneled(const bool & tunneled) {
	mIsTunneled = tunneled;
}

const bool &Http::HttpConnection::isSSL() const {
	return mIsSSL;
}

void Http::HttpConnection::setSSL(const bool & ssl) {
	mIsSSL = ssl;
}

const bool &Http::HttpConnection::isKeepAlive() const {
	return mIsKeepAlive;
}

void Http::HttpConnection::setKeepAlive(const bool & isKeepAlive) {
	mIsKeepAlive = isKeepAlive;
}

}}
