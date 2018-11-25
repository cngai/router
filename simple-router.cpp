/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2017 Alexander Afanasyev
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "simple-router.hpp"
#include "core/utils.hpp"

#include <fstream>

namespace simple_router {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// IMPLEMENT THIS METHOD
void
SimpleRouter::handlePacket(const Buffer& packet, const std::string& inIface)
{
  std::cerr << "Got packet of size " << packet.size() << " on interface " << inIface << std::endl;

  const Interface* iface = findIfaceByName(inIface);
  if (iface == nullptr) {
    std::cerr << "Received packet, but interface is unknown, ignoring" << std::endl;
    return;
  }

  //debugging
  print_hdrs(packet);

  std::cerr << getRoutingTable() << std::endl;

  // FILL THIS IN

  // get ethernet header
  ethernet_hdr header;
  memcpy(&header, packet.data(), sizeof(ethernet_hdr)); //copy e-header from packet to header var

  //get addresses
  std::string packet_address = macToString(packet); //get MAC address of packet
  std::string iface_address = macToString(iface->addr); //get address of interface

  std::string broadcast_address_low = "ff:ff:ff:ff:ff:ff";  //broadcast address lowercase
  std::string broadcast_address_up = "FF:FF:FF:FF:FF:FF";  //broadcast address uppercase

  //REQ 1 - ignore Ethernet frames other than ARP and IPv4
  uint16_t ether_type;
  ether_type = ethertype((const uint8_t*)packet.data());  //get frame type;

  if (ether_type == ethertype_arp){
    std::cerr << "Type is ARP" << std::endl;
    handleARP(packet, iface);
  }
  else if (ether_type == ethertype_ip){
    std::cerr << "Type is IPv4" << std::endl;
    handleIP(packet, iface);
  }
  else {
    std::cerr << "Type is neither ARP nor IPv4. Ignore frame." << std::endl;
    return;
  }


  //REQ 2 - ignore Ethernet frames not destined to router
  //dest. HW address is neither corresponding MAC address of interface nor broadcast address
  if ((packet_address != iface_address) && (packet_address != broadcast_address_low) && (packet_address != broadcast_address_up)){
    std::cerr << "Ethernet frames not destined to router." << std::endl;
    return;
  }
}

//helper function to handle ARP requests/replies
void SimpleRouter::handleARP(const Buffer& packet, const Interface* iface){
  //get ARP header
  arp_hdr* arp_header = (arp_hdr*)(packet.data() + sizeof(ethernet_hdr)); //pointer to beginning of ARP header

  //check to see if ARP request or ARP reply
  uint16_t arp_operation = ntohs(arp_header->arp_op);

  //ARP request
  if (arp_operation == arp_op_request){
    //make sure ARP target address is same as interface address
    if (arp_header->arp_tip != iface->ip){
      std::cerr << "ARP IP address does not match interface IP address." << std::endl;
      return; //drop packet
    }

    //create reply and send back
    uint8_t buff_length = sizeof(ethernet_hdr) + sizeof(arp_hdr);
    Buffer reply_buffer(buff_length);    //create buffer for ARP reply
    uint8_t* arp_reply = (uint8_t *)reply_buffer.data();

    //create reply ethernet header
    ethernet_hdr* e_header_reply = (ethernet_hdr *)arp_reply;   //sets pointer to ethernet header of arp_reply
    memcpy(e_header_reply->ether_shost, iface->addr.data(), ETHER_ADDR_LEN);  //copy interface address to source address
    memcpy(e_header_reply->ether_dhost, &(arp_header->arp_sha), ETHER_ADDR_LEN);  //copy sender address to destination address
    e_header_reply->ether_type = htons(ethertype_arp);  //set ethernet type as ARP

    //create reply ARP header
    arp_hdr* a_header_reply = (arp_hdr *)(arp_reply + sizeof(ethernet_hdr));  //sets pointer to arp header of arp_reply
    a_header_reply->arp_hrd = htons(arp_hrd_ethernet);  //set format of hardware address
    a_header_reply->arp_pro = htons(ethertype_ip);  //set protocol as IP
    a_header_reply->arp_hln = ETHER_ADDR_LEN; //length of hardware address is 6 bytes
    a_header_reply->arp_pln = 4;  //length of protocol address is 4 bytes
    a_header_reply->arp_op = htons(arp_op_reply); //set ARP operation as reply
    memcpy(a_header_reply->arp_sha, iface->addr.data(), ETHER_ADDR_LEN); //copy interface address as sender HW address
    a_header_reply->arp_sip = iface->ip;  //set interface IP address as sender IP address
    memcpy(a_header_reply->arp_tha, &(arp_header->arp_sha), ETHER_ADDR_LEN); //copy ARP request sender HW address as new target HW address
    a_header_reply->arp_tip = arp_header->arp_sip;   //set ARP request sender IP address as new target IP address

    //debugging
    print_hdrs(reply_buffer);

    //send ARP reply back
    sendPacket(reply_buffer, iface->name);
  }
  //ARP reply
  else if (arp_operation == arp_op_reply){
    //record IP-MAC mapping information in ARP cache
    uint32_t sip = arp_header->arp_sip;   //source IP address of ARP reply
    Buffer mac(ETHER_ADDR_LEN);
    memcpy(mac.data(), arp_header->arp_sha, ETHER_ADDR_LEN);  //copy source HW address of ARP reply to 'mac' buffer

    //* 1) Looks up this IP in the request queue. If it is found, returns a pointer
    //*    to the ArpRequest with this IP. Otherwise, returns nullptr.
    //* 2) Inserts this IP to MAC mapping in the cache, and marks it valid.
    std::shared_ptr<ArpRequest> arp_req = m_arp.insertArpEntry(mac, sip);

    // Check if IP is in request queue
    // Given address from ARP, can send pending packets arp request
    // Iterate through packets, based on loop in arp-cache.cpp
    if (arp_req != nullptr) {
      //iterate through list of pending packets in queue
      for (std::list<PendingPacket>::const_iterator pp_iterator = arp_req->packets.begin(); pp_iterator != arp_req->packets.end(); ++pp_iterator) {
        const uint8_t* arp_buff = pp_iterator->packet.data(); //holds Ethernet frame of pending packet
        ethernet_hdr* e_header = (ethernet_hdr *)arp_buff;  //points to ethernet header of frame
        memcpy(e_header->ether_shost, iface->addr.data(), ETHER_ADDR_LEN); //copy interface address as source address
        memcpy(e_header->ether_dhost, arp_header->arp_sha, ETHER_ADDR_LEN); //copy ARP reply's source HW address as new dest address

        //send out all corresponding enqueued packets for the ARP entry
        sendPacket(pp_iterator->packet, pp_iterator->iface);
      }

      //Frees all memory associated with this arp request entry. If this arp request
      //entry is on the arp request queue, it is removed from the queue.
      m_arp.removeRequest(arp_req);
    }

  }
  else{
    std::cerr << "ARP operation is neither a request nor a reply." << std::endl;
    return; //drop packet
  }
}

//helper function to handle IP packets
void SimpleRouter::handleIP(const Buffer& packet, const Interface* iface){
  //get IP packet
  Buffer ip_packet(packet);

  //get IP header
  ip_hdr* ip_header = (ip_hdr*)(ip_packet.data() + sizeof(ethernet_hdr)); //pointer to beginning of IP header

  //verify checksum
  uint16_t cs = ip_header->ip_sum;    //get IP packet checksum
  ip_header->ipsum = 0;
  uint16_t expected_cs = cksum(ip_header, sizeof(ip_hdr));  //expected checksum

  if (cs != expected_cs){
    std::cerr << "Checksum does not match expected checksum" << std::endl;
    return; //drop packet
  }

  //verify min length of IP packet
  if (packet.size() < (sizeof(ethernet_hdr) + sizeof(ip_hdr)){
    std::cerr << "IP packet size smaller than size of ethernet + IP headers" << std::endl;
    return; //drop packet
  }

  if (ip_header->ip_len < sizeof(ip_hdr))){
    std::cerr << "Length of IP packet smaller than IP header" << std::endl;
    return; //drop packet
  }
  


}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// You should not need to touch the rest of this code.
SimpleRouter::SimpleRouter()
  : m_arp(*this)
{
}

void
SimpleRouter::sendPacket(const Buffer& packet, const std::string& outIface)
{
  m_pox->begin_sendPacket(packet, outIface);
}

bool
SimpleRouter::loadRoutingTable(const std::string& rtConfig)
{
  return m_routingTable.load(rtConfig);
}

void
SimpleRouter::loadIfconfig(const std::string& ifconfig)
{
  std::ifstream iff(ifconfig.c_str());
  std::string line;
  while (std::getline(iff, line)) {
    std::istringstream ifLine(line);
    std::string iface, ip;
    ifLine >> iface >> ip;

    in_addr ip_addr;
    if (inet_aton(ip.c_str(), &ip_addr) == 0) {
      throw std::runtime_error("Invalid IP address `" + ip + "` for interface `" + iface + "`");
    }

    m_ifNameToIpMap[iface] = ip_addr.s_addr;
  }
}

void
SimpleRouter::printIfaces(std::ostream& os)
{
  if (m_ifaces.empty()) {
    os << " Interface list empty " << std::endl;
    return;
  }

  for (const auto& iface : m_ifaces) {
    os << iface << "\n";
  }
  os.flush();
}

const Interface*
SimpleRouter::findIfaceByIp(uint32_t ip) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [ip] (const Interface& iface) {
      return iface.ip == ip;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

const Interface*
SimpleRouter::findIfaceByMac(const Buffer& mac) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [mac] (const Interface& iface) {
      return iface.addr == mac;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

const Interface*
SimpleRouter::findIfaceByName(const std::string& name) const
{
  auto iface = std::find_if(m_ifaces.begin(), m_ifaces.end(), [name] (const Interface& iface) {
      return iface.name == name;
    });

  if (iface == m_ifaces.end()) {
    return nullptr;
  }

  return &*iface;
}

void
SimpleRouter::reset(const pox::Ifaces& ports)
{
  std::cerr << "Resetting SimpleRouter with " << ports.size() << " ports" << std::endl;

  m_arp.clear();
  m_ifaces.clear();

  for (const auto& iface : ports) {
    auto ip = m_ifNameToIpMap.find(iface.name);
    if (ip == m_ifNameToIpMap.end()) {
      std::cerr << "IP_CONFIG missing information about interface `" + iface.name + "`. Skipping it" << std::endl;
      continue;
    }

    m_ifaces.insert(Interface(iface.name, iface.mac, ip->second));
  }

  printIfaces(std::cerr);
}


} // namespace simple_router {
