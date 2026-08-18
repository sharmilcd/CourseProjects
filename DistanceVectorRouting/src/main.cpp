#include "node.h"

// Global toggle for testing
bool USE_POISONED_REVERSE = true; 

vector<RoutingNode*> distanceVectorNodes;

void routingAlgo(vector<RoutingNode*> distanceVectorNodes);

int main() {
    int n; 
    cin >> n;
    string name; 
    distanceVectorNodes.clear();
    
    for (int i = 0; i < n; i++) {
        RoutingNode* newnode = new RoutingNode();
        cin >> name;
        newnode->setName(name);
        distanceVectorNodes.push_back(newnode);
    }

    cin >> name;
    
    // NEW: Variables to store the dynamically chosen link to break
    string breakNode1 = "";
    string breakNode2 = "";

    while (name != "EOE") { 
        string token2;
        cin >> token2;

        if (token2 == "EOE") break;

        // PARSER UPGRADE: 
        // If token2 contains a dot (.), it's an IP address -> Normal link setup
        if (token2.find('.') != string::npos) {
            string myeth = token2;
            string oeth, oname;
            int cost;
            cin >> oeth >> oname >> cost; 

            for (int i = 0; i < distanceVectorNodes.size(); i++) {
                if (distanceVectorNodes[i]->getName() == name) {
                    for (int j = 0; j < distanceVectorNodes.size(); j++) {
                        if (distanceVectorNodes[j]->getName() == oname) {
                            distanceVectorNodes[i]->addInterface(myeth, oeth, distanceVectorNodes[j], cost);
                            distanceVectorNodes[i]->addTblEntry(myeth, 0);
                            break;
                        }
                    }
                }
            }
        } 
        // If token2 does NOT have a dot, it's a node label -> The "Break Link" command
        else {
            breakNode1 = name;
            breakNode2 = token2;
        }
        
        cin >> name;
    }

    // Interactive Toggle
    int choice;
    cout << "\nSelect Routing Mode:" << endl;
    cout << "1. Use Poisoned Reverse (Safe)" << endl;
    cout << "0. Disable Poisoned Reverse (Trigger Count-to-Infinity)" << endl;
    cout << "Choice: ";
    cin >> choice;
    USE_POISONED_REVERSE = (choice == 1);

    cout << "\n--- INITIAL CONVERGENCE ---" << endl;
    routingAlgo(distanceVectorNodes);

    /* Dynamically Simulate Link Failure */
    /* Dynamically Simulate Link Failure */
    if (breakNode1 != "" && breakNode2 != "") {
        bool linkFound = false;
        
        // Loop through and attempt to sever the connection on both sides
        for (int i = 0; i < distanceVectorNodes.size(); i++) {
            if (distanceVectorNodes[i]->getName() == breakNode1) {
                // If severLinkTo returns true, mark linkFound as true
                if (distanceVectorNodes[i]->severLinkTo(breakNode2)) {
                    linkFound = true;
                }
            }
            if (distanceVectorNodes[i]->getName() == breakNode2) {
                if (distanceVectorNodes[i]->severLinkTo(breakNode1)) {
                    linkFound = true;
                }
            }
        }
        
        // Check our flag before running the algorithm again
        if (linkFound) {
            cout << "\n--- SIMULATING LINK FAILURE (" << breakNode1 << " to " << breakNode2 << ") ---" << endl;
            routingAlgo(distanceVectorNodes);
        } else {
            cout << "\nNo link found between " << breakNode1 << " and " << breakNode2 << " — skipping link failure." << endl;
        }
        
    } else {
        cout << "\nNo link failure specified in input file." << endl;
    }

    return 0;
}