Name: Christopher Ngai
UID: 404795904

High Level Design of Implementation
 
=========
OVERVIEW
=========

simple-router.cpp implements the router that is being used to handle all packets sent to and from its interfaces.
Its main function is handlePacket() which takes in a Buffer of characters that represent the packet being sent as well
as the name of the incoming interface in which the packet is being received from. handlePacket() receives these packets
and ignores Ethernet frames other than ARP and IPv4. It also ignores any Ethernet frames that are not destined to the router.
However, when the router does receive ARP or IPv4 packets, it must appropriately handle each packet either using the helper
functions handleARP() or handleIP().
	
	The handleARP() function checks to see if the packet is an ARP request or an ARP reply.
If it's an ARP request, then the router creates an ARP request and subsequently sends it back to the sender with the
MAC address of the router's interface. If it's an ARP response, then the router records the IP to MAC address mapping in the
ARP entry cache. If the mapping is already in the cache, then the router iterates through all the packets for the pending
ARP request. If the packet is neiher an ARP request or an ARP response, the router drops the packet.

	The handleIP() function receives the IP packet and verifies the checksum and checks to make sure it meets the minimum
length. The router then determines whether or not the datagram is destined to the router. If it is, then the packet is dropped.
If not, then that means the packet is destined to one of the servers and the router now has the job of forwarding the packet
to the correct MAC address. To do so, it recomputes the checksum and then uses the longest prefix match algorithm implemented
in the lookup() function to find the IP address of the next hop. Once it gets the IP address, it checks to see if the IP to
MAC address mapping is in the ARP cache. If it is, then the router forwards the IP packet to the next hop. If it isn't,
the router queues the packet and then sends an ARP request to get the MAC address of the destination server.

routing-table.cpp essentially implements the routing table of the router. Its main function is the lookup() function, which
uses the longest prefix match algorithm to find the next hop. To do so, I used a helper function called sortMaskLengths()
which takes in two routing table entries and returns true if the mask of the first entry is greater than that of the
second entry. This function is used to sort the routing table by mask length so that the entry with the longest mask
length is at the beginning of the routing table and the entry with the shortest mask length is at the end. I then
iterate through all the routing table entries and check to see if the masked routing table entry destination is equal to
the masked IP address that is inputted. If it is, then the function returns that routing table entry.

arp-cache.cpp handles cache entries and removing stale entries. Its main function is periodicCheckArpRequestsAndCacheEntries()
which checks to see if all the cache entries are still valid and if the ARP requests have been sent 5 or more times. To check
the ARP requests, it iterates through all the requests and first checks to see if the the number of times a specific request
has been sent is 5 or more. If it is, then I iterate through that ARP request's list of pending packets and remove all the
packets. I then remove the ARP request from the list of ARP requests. If not, then the router will try sending that ARP
request again, hoping that it will be received and an ARP response is sent back. It updates the number of times its been sent
if the ARP request isn't successfully acknowledged with an ARP response. To check to see if the cache entries are still valid
and not stale, I iterate through all the ARP entries and if they are marked as invalid, then I remove them from the list of
ARP entries. The ticker() function that is called every second deals with marking ARP entries invalid after 30 seconds.

====================
PROBLEMS & SOLUTIONS
====================