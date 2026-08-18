#ifndef NODE_H
#define NODE_H

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

// Global toggle: when true, sendMsg() applies poisoned reverse.
extern bool USE_POISONED_REVERSE;

/*
  RIP's "infinity". A route costing INFINITY_COST or more is considered
  unreachable. 16 is the value used by RIPv1/v2 (RFC 2453), which also bounds
  how long count-to-infinity can run before a dead route is dropped.
*/
const int INFINITY_COST = 16;

/*
  Each row in the table will have these fields
  dstip:	Destination IP address
  nexthop: 	Next hop on the path to reach dstip
  ip_interface: nexthop is reachable via this interface (a node can have multiple interfaces)
  cost: 	cost of reaching dstip (number of hops)
*/
class RoutingEntry{
 public:
  string dstip, nexthop;
  string ip_interface;
  int cost;
};

/*
 * Class for specifying the sort order of Routing Table Entries
 * while printing the routing tables
 * 
*/
class Comparator{
 public:
  bool operator()(const RoutingEntry &R1,const RoutingEntry &R2){
    if (R1.cost == R2.cost) {
      return R1.dstip.compare(R2.dstip)<0;
    }
    else if(R1.cost > R2.cost) {
      return false;
    }
    else {
      return true;
    }
  }
} ;

/*
  This is the routing table
*/
struct routingtbl {
  vector<RoutingEntry> tbl;
};

/*
  Message format to be sent by a sender
  from: 		Sender's ip
  mytbl: 		Senders routing table
  recvip:		Receiver's ip
*/
class RouteMsg {
 public:
  string from;			// I am sending this message, so it must be me i.e. if A is sending mesg to B then it is A's ip address
  struct routingtbl *mytbl;	// This is routing table of A
  string recvip;		// B ip address that will receive this message
};

/*
  Emulation of network interface. Since we do not have a wire class, 
  we are showing the connection by the pair of IP's
  
  ip: 		Own ip
  connectedTo: 	An address to which above mentioned ip is connected via ethernet.
*/
class NetInterface {
 private:
  string ip;
  string connectedTo; 	//this node is connected to this ip
  int cost; // NEW: Store link cost
  
 public:
  string getip() { return this->ip; }
  string getConnectedIp() { return this->connectedTo; }
  int getCost() { return this->cost; } // NEW
  
  void setip(string ip) { this->ip = ip; }
  void setConnectedip(string ip) { this->connectedTo = ip; }
  void setCost(int c) { this->cost = c; } // NEW
};

/*
  Struct of each node
  name: 	It is just a label for a node
  interfaces: 	List of network interfaces a node have
  Node* is part of each interface, it easily allows to send message to another node
  mytbl: 		Node's routing table
*/
class Node {
 private:
  string name;
  vector<pair<NetInterface, Node*> > interfaces;
 protected:
  struct routingtbl mytbl;
  // NEW: Helper function to get the link cost for a specific receiving IP
  int getLinkCost(string recvip) {
    for (int i = 0; i < interfaces.size(); ++i) {
      if(interfaces[i].first.getip() == recvip) {
        return interfaces[i].first.getCost();
      }
    }
    return 1; // Fallback
  }
  virtual void recvMsg(RouteMsg* msg) {
    cout<<"Base"<<endl;
  }
  bool isMyInterface(string eth) {
    for (int i = 0; i < interfaces.size(); ++i) {
      if(interfaces[i].first.getip() == eth)
	return true;
    }
    return false;
  }
 public:
  void setName(string name){
    this->name = name;
  }

  void updateInterfaceCost(string connip, int newCost) {
    for (int i = 0; i < interfaces.size(); ++i) {
      if (interfaces[i].first.getConnectedIp() == connip) {
        interfaces[i].first.setCost(newCost);
      }
    }
  }

  bool severLinkTo(string neighborName) {
    bool found = false;
    for (int i = 0; i < interfaces.size(); ++i) {
      if (interfaces[i].second->getName() == neighborName) {
        interfaces[i].first.setCost(INFINITY_COST);
        found = true;
      }
    }
    return found;
  }
  
  // MODIFIED: Added 'int cost' parameter
  void addInterface(string ip, string connip, Node *nextHop, int cost) {
    NetInterface eth;
    eth.setip(ip);
    eth.setConnectedip(connip);
    eth.setCost(cost); // NEW: Set the cost
    interfaces.push_back({eth, nextHop});
  }
  
  void addTblEntry(string myip, int cost) {
    RoutingEntry entry;
    entry.dstip = myip;
    entry.nexthop = myip;
    entry.ip_interface = myip;
    entry.cost = cost;
    mytbl.tbl.push_back(entry);
  }

  void updateTblEntry(string dstip, int cost) {
    // to update the dstip hop count in the routing table (if dstip already exists)
    // new hop count will be equal to the cost 
    for (int i=0; i<mytbl.tbl.size(); i++){
      RoutingEntry entry = mytbl.tbl[i];

      if (entry.dstip == dstip) 
        mytbl.tbl[i].cost = cost;

    }

    // remove interfaces 
    for(int i=0; i<interfaces.size(); ++i){
      // if the interface ip is matching with dstip then remove
      // the interface from the list
      if (interfaces[i].first.getConnectedIp() == dstip) {
        interfaces.erase(interfaces.begin() + i);
      }
    }
  }
  
  string getName() {
    return this->name;
  }
  
  struct routingtbl getTable() {
    return mytbl;
  }
  
  void printTable() {
    Comparator myobject;
    sort(mytbl.tbl.begin(),mytbl.tbl.end(),myobject);
    cout<<this->getName()<<":"<<endl;
    for (int i = 0; i < mytbl.tbl.size(); ++i) {
      cout<<mytbl.tbl[i].dstip<<" | "<<mytbl.tbl[i].nexthop<<" | "<<mytbl.tbl[i].ip_interface<<" | "<<mytbl.tbl[i].cost <<endl;
    }
  }
  
  void sendMsg(){
    for (int i = 0; i < interfaces.size(); ++i) {
      struct routingtbl ntbl; // Create a custom table for this neighbor
      string neighborIP = interfaces[i].first.getConnectedIp();

      for (int j = 0; j < mytbl.tbl.size(); ++j) {
        RoutingEntry entry = mytbl.tbl[j];
        
        // POISONED REVERSE: If we route through this neighbor to get to the destination,
        // advertise the cost as infinity (999) to prevent routing loops.
        // POISONED REVERSE TOGGLE
        if (USE_POISONED_REVERSE && entry.nexthop == neighborIP) {
          entry.cost = INFINITY_COST;
        }
        
        ntbl.tbl.push_back(entry);
      }
      
      RouteMsg msg;
      msg.from = interfaces[i].first.getip();
      msg.mytbl = &ntbl;
      msg.recvip = neighborIP;		
      interfaces[i].second->recvMsg(&msg);
    }
  }
  
};

class RoutingNode: public Node {
 public:
  void recvMsg(RouteMsg *msg);
};

#endif // NODE_H
