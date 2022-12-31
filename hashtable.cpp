#include "hashtable.h"

//HashTable::HashTable() : _size(0), _capacity(1024) {
//    // TODO: Replace with own implementation, templates
//    _storage = std::unordered_map<std::string, int> {};
//}
//
//HashTable::HashTable(size_t cap) : _size(0), _capacity(cap) {
//    // TODO: Replace with own implementation, templates
//    _storage = std::unordered_map<std::string, int> {};
//}
//
//bool HashTable::insert(const std::string& key, int value) {
//    // TODO
//    return false;
//}
//
//std::optional<int> HashTable::get(const std::string& key) const {
//    // TODO
//    return std::make_optional<int>(0);
//}
//
//std::optional<int> HashTable::remove(const std::string& key) const {
//    // TODO
//    return std::make_optional<int>(0);
//}
//
//std::vector<std::string> HashTable::getKeys() const {
//    // TODO
//    return std::vector<std::string> {};
//}
//
//size_t HashTable::size() const {
//    return _size;
//}
//
//size_t HashTable::capacity() const {
//    return _capacity;
//}
//
//double HashTable::load_factor() const {
//    // TODO
//    return 0.0;
//}

// #include <iostream>
// #include <unordered_map>
// #include <list>
// 
// // Ein einfacher Datentyp für die Elemente in der Hashtable
// struct Element {
//   int key;
//   int value;
// };
// 
// // Eine Hashtable-Klasse, die verkettete Listen zur Auflösung von Hashkollisionen verwendet
// class Hashtable {
// public:
//   // Konstruktor, der die Größe der Hashtable festlegt
//   Hashtable(int size) : m_size(size) {}
// 
//   // Fügt ein Element mit dem gegebenen Schlüssel und Wert in die Hashtable ein
//   void insert(int key, int value) {
//     // Berechne den Hashwert des Schlüssels und verwende ihn als Index für den Bucket
//     int index = hash(key);
// 
//     // Füge das Element in den entsprechenden Bucket ein
//     m_buckets[index].push_back({key, value});
//   }
// 
//   // Liest das Element mit dem gegebenen Schlüssel aus der Hashtable aus
//   // Gibt ein Paar mit dem Wert des Elements und einem booleschen Wert zurück, der angibt, ob das Element gefunden wurde
//   std::pair<int, bool> read(int key) {
//     // Berechne den
// 
