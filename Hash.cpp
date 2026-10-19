#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

// hash function from the assignment (NOT safe for real passwords)
uint64_t demoHash(const string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h = (h ^ c) * 1099511628211ULL;
    }
    return h;
}

struct User {
    string username;
    string salt;
    uint64_t hash; // demoHash(salt + password + pepper)
};

// same pepper for everyone, it is not saved in the table
const string PEPPER = "myPepper";

uint64_t makeHash(const string& salt, const string& password) {
    return demoHash(salt + password + PEPPER);
}

// ---------- pieces used inside the table ----------

// chaining: each bucket points to a linked list of these
struct Node {
    User user;
    Node* next;
};

// linear probing: each slot is empty, full, or removed
enum Status { EMPTY, FULL, REMOVED };
struct Slot {
    Status status = EMPTY;
    User user;
};

// ---------- the hash table ----------

class HashTable {
private:
    bool useChaining;     // true = chaining, false = linear probing
    int capacity;         // number of buckets
    int count;            // number of users stored
    vector<Node*> chains; // used for chaining
    vector<Slot> slots;   // used for linear probing

    // makes the table bigger and puts every user back in
    void resize() {
        // 1. collect all the users
        vector<User> all;
        for (int i = 0; i < (int) chains.size(); i++) {
            Node* n = chains[i];
            while (n != nullptr) {
                all.push_back(n->user);
                Node* old = n;
                n = n->next;
                delete old;
            }
        }
        for (int i = 0; i < (int) slots.size(); i++) {
            if (slots[i].status == FULL) {
                all.push_back(slots[i].user);
            }
        }

        // 2. make a new empty table that is about twice as big
        cout << "  [REHASH] " << capacity << " buckets -> " << capacity * 2 + 1 << " buckets ("
             << all.size() << " users moved)" << endl;
        capacity = capacity * 2 + 1;
        count = 0;
        chains.assign(capacity, nullptr);
        slots.assign(capacity, Slot());

        // 3. insert everyone again (their buckets change because capacity changed)
        for (int i = 0; i < (int) all.size(); i++) {
            insert(all[i]);
        }
    }

public:
    HashTable(bool chaining, int startCapacity) {
        useChaining = chaining;
        capacity = startCapacity;
        count = 0;
        chains.assign(capacity, nullptr);
        slots.assign(capacity, Slot());
    }

    ~HashTable() {
        for (int i = 0; i < (int) chains.size(); i++) {
            Node* n = chains[i];
            while (n != nullptr) {
                Node* old = n;
                n = n->next;
                delete old;
            }
        }
    }

    double loadFactor() {
        return (double) count / capacity;
    }

    // returns false if the username is already there
    bool insert(User user) {
        if (find(user.username) != nullptr) {
            return false;
        }

        // resize first if adding this user makes the table too full
        double maxLoad = useChaining ? 1.0 : 0.5;
        if ((double) (count + 1) / capacity > maxLoad) {
            resize();
        }

        int index = demoHash(user.username) % capacity;

        if (useChaining) {
            // put the new node at the front of the list
            Node* n = new Node{user, chains[index]};
            chains[index] = n;
        } else {
            // go to the next slot until we find one that isn't full
            while (slots[index].status == FULL) {
                index = (index + 1) % capacity;
            }
            slots[index].user = user;
            slots[index].status = FULL;
        }
        count++;
        return true;
    }

    // returns a pointer to the user, or nullptr if not found
    User* find(string username) {
        int index = demoHash(username) % capacity;

        if (useChaining) {
            Node* n = chains[index];
            while (n != nullptr) {
                if (n->user.username == username) {
                    return &n->user;
                }
                n = n->next;
            }
            return nullptr;
        }

        // linear probing: stop when we hit a slot that was never used
        for (int i = 0; i < capacity; i++) {
            if (slots[index].status == EMPTY) {
                return nullptr;
            }
            if (slots[index].status == FULL && slots[index].user.username == username) {
                return &slots[index].user;
            }
            index = (index + 1) % capacity;
        }
        return nullptr;
    }

    // returns true if the user was found and removed
    bool remove(string username) {
        int index = demoHash(username) % capacity;

        if (useChaining) {
            Node* n = chains[index];
            Node* prev = nullptr;
            while (n != nullptr) {
                if (n->user.username == username) {
                    if (prev == nullptr) {
                        chains[index] = n->next;
                    } else {
                        prev->next = n->next;
                    }
                    delete n;
                    count--;
                    return true;
                }
                prev = n;
                n = n->next;
            }
            return false;
        }

        for (int i = 0; i < capacity; i++) {
            if (slots[index].status == EMPTY) {
                return false;
            }
            if (slots[index].status == FULL && slots[index].user.username == username) {
                slots[index].status = REMOVED; // not EMPTY, so searches keep going past it
                count--;
                return true;
            }
            index = (index + 1) % capacity;
        }
        return false;
    }
};

// ---------- login stuff ----------

void addUser(HashTable& table, string username, string password) {
    User u;
    u.username = username;
    u.salt = to_string(rand()); // random salt for this user
    u.hash = makeHash(u.salt, password);
    table.insert(u);
}

void login(HashTable& table, string username, string password) {
    User* u = table.find(username);
    if (u == nullptr) {
        cout << "  " << username << ": FAILED, unknown user" << endl;
    } else if (makeHash(u->salt, password) != u->hash) {
        cout << "  " << username << ": FAILED, wrong password" << endl;
    } else {
        cout << "  " << username << ": SUCCESS" << endl;
    }
}

void runDemo(bool chaining) {
    cout << endl << "===== " << (chaining ? "Chaining" : "Linear probing") << " =====" << endl;
    HashTable users(chaining, 5); // small start so we can see a rehash

    cout << "Adding 6 users:" << endl;
    addUser(users, "alice", "wonderland42");
    addUser(users, "bob", "hunter2");
    addUser(users, "carol", "correct-horse");
    addUser(users, "dave", "Tr0ub4dor");
    addUser(users, "erin", "p@ssw0rd");
    addUser(users, "frank", "letmein");
    cout << "  load factor = " << users.loadFactor() << endl;

    cout << "Logins:" << endl;
    login(users, "alice", "wonderland42"); // success
    login(users, "bob", "wrongpassword");  // wrong password
    login(users, "mallory", "anything");   // unknown user

    cout << "Removing carol:" << endl;
    users.remove("carol");
    login(users, "carol", "correct-horse"); // now unknown
    login(users, "dave", "Tr0ub4dor");      // still works

    cout << "Adding 7 more users:" << endl;
    for (int i = 0; i < 7; i++) {
        addUser(users, "student" + to_string(i), "pw");
    }
    cout << "  load factor = " << users.loadFactor() << endl;
    login(users, "alice", "wonderland42"); // still works after rehash
}

int main() {
    srand(210);

    runDemo(true);  // chaining
    runDemo(false); // linear probing

    cout << endl << "===== Which is better? (300 users each) =====" << endl;
    HashTable a(true, 5);
    HashTable b(false, 5);
    for (int i = 0; i < 300; i++) {
        addUser(a, "user" + to_string(i), "pw");
        addUser(b, "user" + to_string(i), "pw");
    }
    cout << "Chaining load factor:       " << a.loadFactor() << endl;
    cout << "Linear probing load factor: " << b.loadFactor() << endl;

    return 0;
}
