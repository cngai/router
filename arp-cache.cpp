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

#include "arp-cache.hpp"
#include "core/utils.hpp"
#include "core/interface.hpp"
#include "simple-router.hpp"

#include <algorithm>
#include <iostream>

namespace simple_router {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// IMPLEMENT THIS METHOD
void
ArpCache::periodicCheckArpRequestsAndCacheEntries()
{
  /**
   * IMPLEMENT THIS METHOD
   *
   * This method gets called every second. For each request sent out,
   * you should keep checking whether to resend a request or remove it.
   *
   * Your implementation should follow the following logic
   *
   *     for each request in queued requests:
   *         handleRequest(request)
   *
   *     for each cache entry in entries:
   *         if not entry->isValid
   *             record entry for removal
   *     remove all entries marked for removal
   */

  //now variable represents current time
  auto now = steady_clock::now();

  //iterate through queued requests
  for (std::list<std::shared_ptr<ArpRequest>>::iterator queue_iterator = m_arpRequests.begin(); queue_iterator != m_arpRequests.end(); ){
    //if tried to send arp request 5 or more times, stop re-transmitting, remove pending request,
    //and any packets that are queued for transmission that are associated with the request
    if ((*queue_iterator)->nTimesSent >= MAX_SENT_TIME){
      //iterate through list of pending packets for specific arp request
      for (std::list<PendingPacket>::const_iterator pp_iterator = (*queue_iterator)->packets.begin(); pp_iterator != (*queue_iterator)->packets.end(); ++pp_iterator) {
        ethernet_hdr* pp_e_header = (ethernet_hdr*)(pp_iterator->packet.data()); //set pointer to beginning of packet
        // uint8_t host_unreachable = 1;  //not sure what this is for
        // const Interface * out_iface = m_router.findIfaceByName(pp_iterator->iface);
        // const Interface * in_iface = m_router.findIfaceByMac(Buffer(pp_e_header->ether_dhost, pp_e_header->ether_dhost + ETHER_ADDR_LEN));
      } 
      queue_iterator = m_arpRequests.erase(queue_iterator); //remove pending request
    }
    //send ARP request until ARP reply comes back
    else{
      //send ARP request
      uint8_t buff_length = sizeof(ethernet_hdr) + sizeof(arp_hdr);
      Buffer request_buffer(buff_length);    //create buffer for ARP reply
      uint8_t* arp_req = (uint8_t *)request_buffer.data();

      //create request ethernet header
      ethernet_hdr* e_header_req = (ethernet_hdr *)arp_req;   //sets pointer to ethernet header of arp_req
      const Interface* iface = findIfaceByName((*queue_iterator)->packets.front().iface); //iface name of first packet in queue
      memcpy(e_header_req->ether_shost, iface->addr.data(), ETHER_ADDR_LEN);  //copy interface address to source address
      memcpy(e_header_req->ether_dhost, BroadcastEtherAddr, ETHER_ADDR_LEN);  //copy Broadcast address to destination address
      e_header_req->ether_type = htons(ethertype_arp);  //set ethernet type as ARP

      //create request ARP header
      arp_hdr* a_header_req = (arp_hdr*)(arp_req + sizeof(ethernet_hdr));  //sets point to arp header of arp_req
      a_header_req->arp_hrd = htons(arp_hrd_ethernet);  //set format of hardware address
      a_header_req->arp_pro = htons(ethertype_ip);  //set protocol as IP
      a_header_req->arp_hln = ETHER_ADDR_LEN; //length of hardware address is 6 bytes
      a_header_req->arp_pln = 4;  //length of protocol address is 4 bytes
      a_header_req->arp_op = htons(arp_op_request); //set ARP operation as request
      memcpy(a_header_req->arp_sha, iface->addr.data(), ETHER_ADDR_LEN); //copy IP interface address as sender HW address
      a_header_req->arp_sip = iface->ip;  //set IP interface address as sender IP address
      memcpy(a_header_req->arp_tha, BroadcastEtherAddr, ETHER_ADDR_LEN); //copy Broadcast address as new target HW address
      a_header_req->arp_tip = (*queue_iterator)->ip_dst;   //set IP packet destination address as new target IP address

      //debugging
      print_hdrs(request_buffer);

      //send ARP request back
      m_router.sendPacket(request_buffer, (*queue_iterator)->packets.front().iface);

      //update information
      (*queue_iterator)->timeSent = now;
      (*queue_iterator)->nTimesSent = nTimesSent + 1;

      //move on to next pending request
      ++queue_iterator;
    }
  }

  //iterate through arp entries 
  for (std::list<std::shared_ptr<ArpEntry>>::iterator ae_iterator = m_cacheEntries.begin(); ae_iterator != m_cacheEntries.end();){
    //if arp entry is not valid, erase it form the cache
    if (!(*ae_iterator)->isValid) {
      ae_iterator = m_cacheEntries.erase(ae_iterator);
    }
    //otherwise, continue
    else{
      ++ae_iterator;
    }
  }
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// You should not need to touch the rest of this code.

ArpCache::ArpCache(SimpleRouter& router)
  : m_router(router)
  , m_shouldStop(false)
  , m_tickerThread(std::bind(&ArpCache::ticker, this))
{
}

ArpCache::~ArpCache()
{
  m_shouldStop = true;
  m_tickerThread.join();
}

std::shared_ptr<ArpEntry>
ArpCache::lookup(uint32_t ip)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto& entry : m_cacheEntries) {
    if (entry->isValid && entry->ip == ip) {
      return entry;
    }
  }

  return nullptr;
}

std::shared_ptr<ArpRequest>
ArpCache::queueRequest(uint32_t ip, const Buffer& packet, const std::string& iface)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto request = std::find_if(m_arpRequests.begin(), m_arpRequests.end(),
                           [ip] (const std::shared_ptr<ArpRequest>& request) {
                             return (request->ip == ip);
                           });

  if (request == m_arpRequests.end()) {
    request = m_arpRequests.insert(m_arpRequests.end(), std::make_shared<ArpRequest>(ip));
  }

  // Add the packet to the list of packets for this request
  (*request)->packets.push_back({packet, iface});
  return *request;
}

void
ArpCache::removeRequest(const std::shared_ptr<ArpRequest>& entry)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_arpRequests.remove(entry);
}

std::shared_ptr<ArpRequest>
ArpCache::insertArpEntry(const Buffer& mac, uint32_t ip)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto entry = std::make_shared<ArpEntry>();
  entry->mac = mac;
  entry->ip = ip;
  entry->timeAdded = steady_clock::now();
  entry->isValid = true;
  m_cacheEntries.push_back(entry);

  auto request = std::find_if(m_arpRequests.begin(), m_arpRequests.end(),
                           [ip] (const std::shared_ptr<ArpRequest>& request) {
                             return (request->ip == ip);
                           });
  if (request != m_arpRequests.end()) {
    return *request;
  }
  else {
    return nullptr;
  }
}

void
ArpCache::clear()
{
  std::lock_guard<std::mutex> lock(m_mutex);

  m_cacheEntries.clear();
  m_arpRequests.clear();
}

void
ArpCache::ticker()
{
  while (!m_shouldStop) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
      std::lock_guard<std::mutex> lock(m_mutex);

      auto now = steady_clock::now();

      for (auto& entry : m_cacheEntries) {
        if (entry->isValid && (now - entry->timeAdded > SR_ARPCACHE_TO)) {
          entry->isValid = false;
        }
      }

      periodicCheckArpRequestsAndCacheEntries();
    }
  }
}

std::ostream&
operator<<(std::ostream& os, const ArpCache& cache)
{
  std::lock_guard<std::mutex> lock(cache.m_mutex);

  os << "\nMAC            IP         AGE                       VALID\n"
     << "-----------------------------------------------------------\n";

  auto now = steady_clock::now();
  for (const auto& entry : cache.m_cacheEntries) {

    os << macToString(entry->mac) << "   "
       << ipToString(entry->ip) << "   "
       << std::chrono::duration_cast<seconds>((now - entry->timeAdded)).count() << " seconds   "
       << entry->isValid
       << "\n";
  }
  os << std::endl;
  return os;
}

} // namespace simple_router
