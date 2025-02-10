#pragma once

#include <cstdint>
#include <vector>

namespace dfd {

/*
 * DataPacket format:
 * [left-encode of full packet length][left-encode of packet number][data]
 */
using DataPacket = std::vector<uint8_t>;

/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * dataToPackets
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Converts raw data read in from a file to packets that can be transmitted.
 *    The file_buff should be obtained from fileParsing.
 *
 * Takes:
 * -> file_buff:
 *    The raw file data.
 * -> packed_buff:
 *    A container to store the formatted packets. Any data in this container
 *    will be cleared prior to writing the packets in.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
int dataToPackets(const std::vector<uint8_t>& file_buff,
                  std::vector<DataPacket>&    packet_buff
                 ); 
/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * packetsToData
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Description:
 * -> Converts recieved packets back into raw file data. Packets do not need to
 *    be ordered prior to passing them to this function, it will order the file
 *    data according to the packet headers, placing the result into file_buff.
 *
 * Takes:
 * -> packet_buff:
 *    A collection of recieved packets. 
 * -> file_buff:
 *    The raw file data.
 *
 * Returns:
 * -> On success:
 *    EXIT_SUCCESS
 * -> On failure:
 *    EXIT_FAILURE
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */

int packetsToData(const std::vector<DataPacket>& packet_buff,
                  std::vector<uint8_t>&          file_buff
                 );


}
