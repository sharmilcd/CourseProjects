#include "node.h"
#include <iostream>

using namespace std;

void printRT(vector<RoutingNode*> nd){
/*Print routing table entries*/
	for (int i = 0; i < nd.size(); i++) {
	  nd[i]->printTable();
	}
}



void routingAlgo(vector<RoutingNode*> nd){
  bool saturation = false;
  int rounds = 0;

  // Run until the tables stop changing
  while (!saturation) {
    rounds++;
    saturation = true; // Assume we are done, unless we prove otherwise below

    // 1. Take a snapshot of all routing tables BEFORE the round starts
    vector<routingtbl> previousTables;
    for (RoutingNode* node : nd) {
      previousTables.push_back(node->getTable());
    }

    // 2. Nodes send messages (Toggle logic)
    if (USE_POISONED_REVERSE) {
      // Normal order for Poisoned Reverse
      for (RoutingNode* node : nd) {
        node->sendMsg();
      }
    } else {
      // Reversed order to force the Count-to-Infinity race condition
      for (int i = nd.size() - 1; i >= 0; --i) {
        nd[i]->sendMsg();
      }
    }

    // 3. Compare the new tables with the snapshot to see if anything changed
    for (int i = 0; i < nd.size(); ++i) {
      routingtbl currentTable = nd[i]->getTable();
      routingtbl prevTable = previousTables[i];

      // Did a node discover a brand new route?
      if (currentTable.tbl.size() != prevTable.tbl.size()) {
        saturation = false;
        break; 
      }

      // Did an existing route change its cost or next-hop?
      for (int j = 0; j < currentTable.tbl.size(); ++j) {
        if (currentTable.tbl[j].cost != prevTable.tbl[j].cost ||
            currentTable.tbl[j].nexthop != prevTable.tbl[j].nexthop) {
          saturation = false;
          break;
        }
      }
      
      if (!saturation) break; // Break the outer comparison loop early
    }
  }

  // Print the results
  cout << "--> Algorithm converged in " << rounds << " rounds." << endl;
  printf("Printing the routing tables after the convergence \n");
  printRT(nd);
}

void RoutingNode::recvMsg(RouteMsg *msg) {
  //your code here
 
  // Traverse the routing table in the message.
  // Check if entries present in the message table is closer than already present 
  // entries.
  // Update entries.
 
  routingtbl *recvRoutingTable = msg->mytbl;
  // NEW: Find the actual cost of the link this message arrived on
  int linkCost = getLinkCost(msg->recvip);
  for (RoutingEntry entry : recvRoutingTable->tbl) {
    // Check routing entry

    bool entryExists = false;
    for ( int i=0; i<mytbl.tbl.size(); ++i) {
      RoutingEntry myEntry = mytbl.tbl[i];
      //printf("i=%d, nodeRT.cost=%d, DV.cost=%d\n",i, myEntry.cost, entry.cost );
      if (myEntry.dstip == entry.dstip){
        entryExists = true;
        
        // FORCED UPDATE RULE: 
        // 1. If this message is from our current next-hop, we MUST update (even if worse).
        // 2. Or, if this is a strictly cheaper path from someone else.
        if (myEntry.nexthop == msg->from || myEntry.cost > entry.cost + linkCost) {
          
          // Calculate new cost, but cap it at 999 to prevent overflow
          int newCost = entry.cost + linkCost;
          if (entry.cost >= INFINITY_COST) {
             newCost = INFINITY_COST;
          }

          // Only update if the cost actually changed or the nexthop changed
          if (myEntry.cost != newCost || myEntry.nexthop != msg->from) {
              myEntry.cost = newCost;
              myEntry.nexthop = msg->from;
              mytbl.tbl[i] = myEntry;
          }
        }
      }
    }
    if (!entryExists) {
      // add the new entry
      RoutingEntry newEntry;
      newEntry.dstip = entry.dstip;
      newEntry.nexthop = msg->from;
      newEntry.ip_interface = msg->recvip;
      // MODIFIED: Use linkCost instead of 1
      newEntry.cost = entry.cost + linkCost;
      mytbl.tbl.push_back(newEntry);
    }
  }
 
}


