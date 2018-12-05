Name: Christopher Ngai
UID: 404795904

High Level Design of Implementation
 
=========
Overview
=========

simple-router.cpp implements the router that is being used to handle all packets sent to and from its interfaces.
Its main function is handlePacket() which takes in a Buffer of characters that represent the packet being sent as well
as the name of the incoming interface in which the packet is being received from. handlePacket() receives these packets
and ignores Ethernet frames other than ARP and IPv4. It also ignores any Ethernet frames that are not destined to the router.
However, when the router does receive ARP or IPv4 packets, it must appropriately handle each packet either using the helper
functions handleARP() or handleIP().