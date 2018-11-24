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

void SimpleRouter::handleARP(const Buffer& packet, const Interface* iface){
  //get ARP header
  arp_hdr* arp_header = (arp_hdr*)(packet.data() + sizeof(ethernet_hdr)); //pointer to beginning of ARP header

  //check to see if ARP request or ARP reply
  uint16_t arp_operation = ntohs(arp_header->arp_op);

  //ARP request
  if (arp_operation == arp_op_request){

  }
  //ARP reply
  else if (arp_operation == arp_op_reply){
    
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
