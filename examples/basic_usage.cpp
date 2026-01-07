/**
 * @file basic_usage.cpp
 * @brief Basic usage example for RTTM C++20 reflection library
 * 
 * This example demonstrates:
 * - Type registration with properties and methods
 * - Dynamic object creation
 * - Property access (type-safe and dynamic)
 * - Method invocation
 * - Inheritance support
 * - Container reflection
 * - Error handling
 */

#include "RTTM/RTTM.hpp"
#include <iostream>
#include <vector>
#include <map>

using namespace rttm;
using namespace rttm::detail;

// ============================================================================
// Example Classes
// ============================================================================

// Simple class for basic demonstration
class Person {
public:
    std::string name;
    int age = 0;
    
    Person() = default;
    Person(const std::string& n, int a) : name(n), age(a) {}
    
    std::string greeting() const { return "Hello, I'm " + name; }
    void setAge(int a) { age = a; }
    int getAge() const { return age; }
};

// Base class for inheritance demo
class Animal {
public:
    std::string species;
    int age = 0;
    
    Animal() = default;
    
    void setAge(int a) { age = a; }
    int getAge() const { return age; }
};

// Derived class
class Dog : public Animal {
public:
    std::string name;
    std::string breed;
    
    Dog() { species = "Canine"; }
    
    std::string speak() const { return "Woof! I'm " + name; }
    void rename(const std::string& newName) { name = newName; }
};

// Class with containers for container reflection demo
class GamePlayer {
public:
    std::string playerName;
    int level = 1;
    std::vector<int> scores;
    std::map<std::string, int> inventory;
    
    GamePlayer() = default;
    
    void addScore(int score) { scores.push_back(score); }
    int getScoreCount() const { return static_cast<int>(scores.size()); }
};

// ============================================================================
// Type Registration
// ============================================================================

RTTM_REGISTRATION {
    // Register Person class
    Registry<Person>()
        .property("name", &Person::name)
        .property("age", &Person::age)
        .method("greeting", &Person::greeting)
        .method("setAge", &Person::setAge)
        .method("getAge", &Person::getAge);
    
    // Register Animal base class
    Registry<Animal>()
        .property("species", &Animal::species)
        .property("age", &Animal::age)
        .method("setAge", &Animal::setAge)
        .method("getAge", &Animal::getAge);
    
    // Register Dog with inheritance
    Registry<Dog>()
        .base<Animal>()  // Inherit Animal's properties and methods
        .property("name", &Dog::name)
        .property("breed", &Dog::breed)
        .method("speak", &Dog::speak)
        .method("rename", &Dog::rename);
    
    // Register GamePlayer with containers
    Registry<GamePlayer>()
        .property("playerName", &GamePlayer::playerName)
        .property("level", &GamePlayer::level)
        .property("scores", &GamePlayer::scores)
        .property("inventory", &GamePlayer::inventory)
        .method("addScore", &GamePlayer::addScore)
        .method("getScoreCount", &GamePlayer::getScoreCount);
}

// ============================================================================
// Example Functions
// ============================================================================

void demonstrateBasicUsage() {
    std::cout << "=== Basic Usage ===" << std::endl;
    
    // Get type handle and create instance
    auto personType = RType::get<Person>();
    personType->create();
    
    // Type-safe property access
    personType->property<std::string>("name") = "Alice";
    personType->property<int>("age") = 30;
    
    std::cout << "Name: " << personType->property<std::string>("name") << std::endl;
    std::cout << "Age: " << personType->property<int>("age") << std::endl;
    
    // Method invocation
    std::string greeting = personType->invoke<std::string>("greeting");
    std::cout << "Greeting: " << greeting << std::endl;
    
    // Call method with arguments
    personType->invoke<void>("setAge", 31);
    int newAge = personType->invoke<int>("getAge");
    std::cout << "New age: " << newAge << std::endl;
    
    std::cout << std::endl;
}

void demonstrateInheritance() {
    std::cout << "=== Inheritance ===" << std::endl;
    
    auto dogType = RType::get<Dog>();
    dogType->create();
    
    // Set derived class properties
    dogType->property<std::string>("name") = "Buddy";
    dogType->property<std::string>("breed") = "Golden Retriever";
    
    // Access base class property through derived class
    std::cout << "Species (from base): " << dogType->property<std::string>("species") << std::endl;
    std::cout << "Name: " << dogType->property<std::string>("name") << std::endl;
    std::cout << "Breed: " << dogType->property<std::string>("breed") << std::endl;
    
    // Call derived class method
    std::string bark = dogType->invoke<std::string>("speak");
    std::cout << "Dog says: " << bark << std::endl;
    
    // Call base class method
    dogType->invoke<void>("setAge", 5);
    int age = dogType->invoke<int>("getAge");
    std::cout << "Age (via base method): " << age << std::endl;
    
    // List all properties (including inherited)
    std::cout << "All properties:" << std::endl;
    for (const auto& propName : dogType->property_names()) {
        std::cout << "  - " << propName << std::endl;
    }
    
    std::cout << std::endl;
}

void demonstrateErrorHandling() {
    std::cout << "=== Error Handling ===" << std::endl;
    
    // Try to get unregistered type
    try {
        auto unknownType = RType::get("UnknownType");
    } catch (const TypeNotRegisteredError& e) {
        std::cout << "Caught TypeNotRegisteredError: " << e.what() << std::endl;
    }
    
    // Try to access non-existent property
    try {
        auto personType = RType::get<Person>();
        personType->create();
        personType->property<int>("nonexistent");
    } catch (const PropertyNotFoundError& e) {
        std::cout << "Caught PropertyNotFoundError: " << e.what() << std::endl;
    }
    
    // Try to use object before creation
    try {
        auto personType = RType::get<Person>();
        // Note: not calling create()
        personType->property<std::string>("name");
    } catch (const ObjectNotCreatedError& e) {
        std::cout << "Caught ObjectNotCreatedError: " << e.what() << std::endl;
    }
    
    std::cout << std::endl;
}

void demonstrateContainerReflection() {
    std::cout << "=== Container Reflection ===" << std::endl;
    
    auto playerType = RType::get<GamePlayer>();
    playerType->create();
    
    // Set basic properties
    playerType->property<std::string>("playerName") = "Hero";
    playerType->property<int>("level") = 10;
    
    std::cout << "Player: " << playerType->property<std::string>("playerName") << std::endl;
    std::cout << "Level: " << playerType->property<int>("level") << std::endl;
    
    // Check container property categories
    std::cout << "Is 'scores' a sequential container? " 
              << (playerType->is_sequential_container("scores") ? "yes" : "no") << std::endl;
    std::cout << "Is 'inventory' an associative container? " 
              << (playerType->is_associative_container("inventory") ? "yes" : "no") << std::endl;
    
    // Direct access to vector container
    auto& scores = playerType->property<std::vector<int>>("scores");
    scores.push_back(100);
    scores.push_back(250);
    scores.push_back(500);
    
    std::cout << "Scores: ";
    for (int s : scores) {
        std::cout << s << " ";
    }
    std::cout << std::endl;
    
    // Use container interface for vector
    auto scoresContainer = make_sequential_container(&scores);
    std::cout << "Scores count (via container interface): " << scoresContainer->size() << std::endl;
    
    // Add element via container interface
    scoresContainer->push_back(std::any{750});
    std::cout << "After push_back, scores count: " << scoresContainer->size() << std::endl;
    
    // Iterate via container interface
    std::cout << "Iterating scores: ";
    auto it = scoresContainer->begin();
    while (it->has_current()) {
        std::cout << it->current()->as<int>() << " ";
        it->next();
    }
    std::cout << std::endl;
    
    // Direct access to map container
    auto& inventory = playerType->property<std::map<std::string, int>>("inventory");
    inventory["sword"] = 1;
    inventory["potion"] = 5;
    inventory["gold"] = 100;
    
    // Use container interface for map
    auto inventoryContainer = make_associative_container(&inventory);
    std::cout << "Inventory size: " << inventoryContainer->size() << std::endl;
    
    // Check if item exists
    std::cout << "Has 'potion'? " 
              << (inventoryContainer->contains(std::any{std::string{"potion"}}) ? "yes" : "no") << std::endl;
    
    // Find item value
    auto potionCount = inventoryContainer->find(std::any{std::string{"potion"}});
    if (potionCount) {
        std::cout << "Potion count: " << potionCount->as<int>() << std::endl;
    }
    
    // Insert new item via container interface
    inventoryContainer->insert(std::string{"shield"}, std::any{2});
    std::cout << "After insert, inventory size: " << inventoryContainer->size() << std::endl;
    
    // Iterate map via container interface
    std::cout << "Inventory contents:" << std::endl;
    auto mapIt = inventoryContainer->begin();
    while (mapIt->has_current()) {
        std::cout << "  " << mapIt->key()->as<std::string>() 
                  << ": " << mapIt->value()->as<int>() << std::endl;
        mapIt->next();
    }
    
    std::cout << std::endl;
}

void demonstrateDynamicTypeAccess() {
    std::cout << "=== Dynamic Type Access ===" << std::endl;
    
    // Get type by name string
    auto personType = RType::get("Person");
    personType->create();
    
    // Set properties
    personType->property<std::string>("name") = "DynamicPerson";
    personType->property<int>("age") = 25;
    
    // Dynamic property access (returns RType)
    auto nameProperty = personType->property("name");
    std::cout << "Name via dynamic access: " << nameProperty->as<std::string>() << std::endl;
    
    // Check type information
    std::cout << "Type name: " << personType->type_name() << std::endl;
    std::cout << "Is valid: " << (personType->is_valid() ? "yes" : "no") << std::endl;
    std::cout << "Is class: " << (personType->is_class() ? "yes" : "no") << std::endl;
    
    // List all methods
    std::cout << "All methods:" << std::endl;
    for (const auto& methodName : personType->method_names()) {
        std::cout << "  - " << methodName << std::endl;
    }
    
    std::cout << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "RTTM C++20 Reflection Library - Basic Usage Example" << std::endl;
    std::cout << "====================================================" << std::endl << std::endl;
    
    demonstrateBasicUsage();
    demonstrateInheritance();
    demonstrateContainerReflection();
    demonstrateErrorHandling();
    demonstrateDynamicTypeAccess();
    
    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}
