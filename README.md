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
the masked IP address that is inputted. If it is, then the function returns that routing table entry because it should already
be the longest matching prefix due to the sorted routing table.

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

	One problem that I ran into was figuring out when to use ntohs() and when to use htons() when trying to convert values
between host and network byte order. I was able to solve this issue by looking up the documentation of both of these functions
and discovered that ntohs() converts an unsigned short integer from network byte order to host byte order while htons() does
the opposite conversion. In terms of debugging, I would printe the headers using print_hdrs() after establishing the ethernet
and ARP headers to see whether certain constants like the ethertype or the ARP operation were in the correct and consistent
format. I found that most of the constants needed to convert the constant short from host to network byte order.

	Another problem that I discovered was figuring out how to implement the longest matching prefix algorithm in the lookup()
function. At first I tried to iterate through all the routing table entries to and storing the routing table entry with the
current longest matching prefix in a temporary variable and returning that temporary variable after iterating through the
entire table. But I found that it was more efficient to use the sort() function to sort the list of routing table entries with
the help of a helper function that I called sortMaskLengths(). This helper function, as described above, allows the routing
table to be reordered and sorted from longest mask to shortest mask. This method allows the router to iterate through the routing
table and immediately return the first matching prefix because it will presumably be the longest.

	I had another issue when trying to remove an ARP request from the list of ARP requests after 5 consecutive requests were sent.
I initially thought that we were supposed use the removeRequest() function because that helper function says that it will remove
it from the queue. However, when looking at the implementation of the helper function, I saw that it used the remove() function
which only transforms the range of the array of elements and does not actually remove the actual ARP request. My program eventually
entered deadlock because the removeRequest() function is locked with a mutex. I learned that this function is actually used to remove
ARP requests after getting an ARP reply and sending its corresponding pending packets. In order to remove an ARP request from the list
of ARP requests, I ended up using the erase() function instead.

	Finally, I did not initially understand how to check the router's ARP cache. I initially tried many permutations of the
'client arp 10.0.1.1' command that was listed on the Project FAQ, but realized I was only able to get the ARP entries for
the client or the two servers. I then realized I had to open another terminal outside of mininet and the router and use the
show-arp.py script while the router was running. This enabled me to view all the cache entries for the router and was able
to test if my ARP entries were being properly stored and deleted after 30 seconds. This problem was unclear to me until the
TA sent out an email explaining that that script was available to view the ARP cache.