// GCC
// https://www.mycompiler.io/view/1BTkenQhGFI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ptr(type, value) _Generic((value), \
    char*: (void *)(type)(value), \
    default: (void *)&(type){value} \
)

// Rust's 'str' type (immutable string slice)
typedef const char *str;

// Define the structure for a node in the B-tree
typedef struct _MapNode *MapNode;
struct _MapNode {
    void *key;
    void *value;
    MapNode left;
    MapNode right;
};

// Define the BTreeMap structure
typedef struct _BTreeMap *BTreeMap;
struct _BTreeMap {
    MapNode root;
    size_t size;
    int (*compare)(const void *, const void *); // Function pointer for comparison function
};

// Comparison function for float keys
#define GENERATE_COMPARE_FUNC(type) \
    int type##_compare(const void *a, const void *b) { \
        type fa = *(const type *)a; \
        type fb = *(const type *)b; \
        return (fa > fb) - (fa < fb); \
    }

// Comparison function for str keys
int str_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// Function to create a new node with a key and value
MapNode btreemap_node(void *key, void *value) {
    MapNode node = (MapNode)malloc(sizeof(struct _MapNode));
    if (node != NULL) {
        node->key = key;
        node->value = value;
        node->left = NULL;
        node->right = NULL;
    }
    return node;
}

// Function to create a new BTreeMap
#define btreemap_new(key_type, value_type) ({           \
    BTreeMap map = (BTreeMap)malloc(sizeof(struct _BTreeMap)); \
    if (map != NULL) {                                  \
        map->root = NULL;                               \
        map->size = 0;                                  \
        if (strcmp(#key_type, "str") == 0) {           \
            map->compare = str_compare;                 \
        } else {                                        \
            GENERATE_COMPARE_FUNC(key_type)             \
            map->compare = key_type##_compare;          \
        }                                               \
    }                                                   \
    map;                                                \
})

// Function to insert a key-value pair into the BTreeMap
void btreemap_insert_helper(MapNode *root, void *key, void *value, int (*compare)(const void *, const void *)) {
    if (*root == NULL) {
        *root = btreemap_node(key, value);
    } else {
        if (compare(key, (*root)->key) < 0) {
            btreemap_insert_helper(&((*root)->left), key, value, compare);
        } else if (compare(key, (*root)->key) > 0) {
            btreemap_insert_helper(&((*root)->right), key, value, compare);
        } else {
            // Key already exists, update the value
            (*root)->value = value;
        }
    }
}

void btreemap_insert(BTreeMap map, void *key, void *value) {
    btreemap_insert_helper(&(map->root), key, value, map->compare);
    map->size++;
}

// Function to remove a key-value pair from the BTreeMap
MapNode btreemap_remove_helper(MapNode root, void *key, int (*compare)(const void *, const void *)) {
    if (root == NULL) {
        return NULL;
    }

    if (compare(key, root->key) < 0) {
        root->left = btreemap_remove_helper(root->left, key, compare);
    } else if (compare(key, root->key) > 0) {
        root->right = btreemap_remove_helper(root->right, key, compare);
    } else {
        // Found the node to remove
        if (root->left == NULL) {
            MapNode temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            MapNode temp = root->left;
            free(root);
            return temp;
        }

        // Node with two children: Get the inorder successor (smallest in the right subtree)
        MapNode temp = root->right;
        while (temp->left != NULL) {
            temp = temp->left;
        }

        // Copy the inorder successor's content to this node
        root->key = temp->key;
        root->value = temp->value;

        // Delete the inorder successor
        root->right = btreemap_remove_helper(root->right, temp->key, compare);
    }
    return root;
}

void btreemap_remove(BTreeMap map, void *key) {
    if (map->root == NULL) {
        return;
    }
    map->root = btreemap_remove_helper(map->root, key, map->compare);
    map->size--;
}

// Function to get the number of elements in the BTreeMap
size_t btreemap_len(BTreeMap map) {
    return map->size;
}

// Function to search for a key in the BTreeMap
void *btreemap_search_helper(MapNode root, void *key, int (*compare)(const void *, const void *)) {
    if (root == NULL) {
        return NULL;
    }
    int cmp = compare(key, root->key);
    if (cmp == 0) {
        return root->value;
    } else if (cmp < 0) {
        return btreemap_search_helper(root->left, key, compare);
    } else {
        return btreemap_search_helper(root->right, key, compare);
    }
}

void *btreemap_get(BTreeMap map, void *key) {
    return btreemap_search_helper(map->root, key, map->compare);
}

// Function to clear the BTreeMap
void btreemap_clear_helper(MapNode root) {
    if (root == NULL) {
        return;
    }
    btreemap_clear_helper(root->left);
    btreemap_clear_helper(root->right);
    free(root);
}

void btreemap_clear(BTreeMap map) {
    btreemap_clear_helper(map->root);
    map->root = NULL;
    map->size = 0;
}

// Function to perform inorder traversal and apply a function to each node
void btreemap_foreach_helper(MapNode root, void (*func)(void *key, void *value)) {
    if (root == NULL) {
        return;
    }
    btreemap_foreach_helper(root->left, func);
    func(root->key, root->value);
    btreemap_foreach_helper(root->right, func);
}

// Function to iterate over all elements in the BTreeMap and apply a function to each node
void btreemap_foreach(BTreeMap map, void (*func)(void *key, void *value)) {
    btreemap_foreach_helper(map->root, func);
}

// Example usage:
void print_node(void *key, void *value) {
    printf("Key: %s, Value: %s\n", (str)key, (str)value);
}

int main() {
    // Example usage
    BTreeMap map = btreemap_new(str, str);

    // Inserting key-value pairs with string keys
    btreemap_insert(map, "10", ptr(str, "Value1"));
    btreemap_insert(map, "20", "Value2");
    btreemap_insert(map, "30", "Value3");
    
    // Getting the length of the map
    printf("Size of map: %zu\n", btreemap_len(map));

    // Getting a value by key
    printf("Value for key 10: %s\n", (str)btreemap_get(map, "10"));

    // Removing a key-value pair
    btreemap_remove(map, "10");
    printf("Size after removal: %zu\n", btreemap_len(map));

    // Using foreach to print all key-value pairs in the BTreeMap
    btreemap_foreach(map, print_node);

    // Clearing the map
    btreemap_clear(map);
    printf("Size after clearing: %zu\n", btreemap_len(map));

    free(map);

    return 0;
}
