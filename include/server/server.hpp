#pragma once

#include <cstdint>
#include <string>

namespace dfd {

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * run_server
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Main server startup point. Initializes a server, its database, and will
 *    start listening forever for client requests.
 *
 * Takes:
 * -> ip:
 *    The interface to supply other servers in the network to form a network.
 * -> port:
 *    The port to open the main listener on.
 * -> connect_to:
 *    A (possibly empty) IP for a server to connect to and receive a database
 *    copy from.
 * -> connect_port:
 *    A (possibly empty) port for a server to connect to and receive a database
 *    copy from.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
void run_server(const std::string& ip,
                const uint16_t     port, 
                const std::string& connect_ip, 
                const uint16_t     connect_port);

} //namespace dfd
