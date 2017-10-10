// Implementation of the ServerSocket class

#include "ServerSocket.h"
#include "SocketException.h"
#include <stdio.h>
#include <string>
#include <cstring>


ServerSocket::ServerSocket ( int port )
{
  if ( ! Socket::create() )
    {
      throw SocketException ( "Could not create server socket." );
    }

  if ( ! Socket::bind ( port ) )
    {
      throw SocketException ( "Could not bind to port." );
    }

  if ( ! Socket::listen() )
    {
      throw SocketException ( "Could not listen to socket." );
    }

}

ServerSocket::ServerSocket ( )
{
}

ServerSocket::~ServerSocket()
{
}


const ServerSocket& ServerSocket::operator << ( const std::string& s ) const
{
  if ( ! Socket::send ( s ) )
    {
      throw SocketException ( "Could not write to socket." );
    }

  return *this;

}


const ServerSocket& ServerSocket::operator >> ( std::string& s ) const
{
  if ( ! Socket::recv ( s ) )
    {
      throw SocketException ( "Could not read from socket." );
    }

  return *this;
}

void ServerSocket::accept ( ServerSocket& sock )
{
  if ( ! Socket::accept ( sock ) )
    {
      throw SocketException ( "Could not accept socket." );
    }
}

std::string ServerSocket::client_ip () {
  sockaddr_in c = client_information();
  char *clientip = new char[20];
  strcpy(clientip, inet_ntoa(c.sin_addr));
  std::string result(clientip);
  return result;
}

void ServerSocket::close_socket() {
  Socket::close_socket();
}
