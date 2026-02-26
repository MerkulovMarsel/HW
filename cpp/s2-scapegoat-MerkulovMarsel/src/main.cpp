#include "tree/Tree.hpp"
#include <iostream>

int main() {
    Scapegoat tree;
    tree.contains(0);
    std::cout << tree.insert(2) << "\n";
    //std::cout << tree.contains(2) << "\n";
    //std::cout << tree.insert(2) << "\n";
    std::cout << tree.size() << "\n";
    static_assert(std::forward_iterator_tag))

    std::cout << tree.insert(5) << "\n";
    //std::cout << tree.contains(5) << "\n";
    //std::cout << tree.insert(5) << "\n";
    std::cout << tree.size() << "\n";

    std::cout << tree.insert(7) << "\n";
    //std::cout << tree.contains(7) << "\n";
    //std::cout << tree.insert(7) << "\n";
    std::cout << tree.size() << "\n";

    std::cout << tree.insert(3) << "\n";
    //std::cout << tree.contains(3) << "\n";
    //std::cout << tree.insert(3) << "\n";
    std::cout << tree.size() << "\n";

    std::cout << tree.insert(9) << "\n";
    ///std::cout << tree.contains(1) << "\n";
    //std::cout << tree.insert(1) << "\n";
    std::cout << tree.size() << "\n";

    std::cout << tree.insert(10) << "\n";
    //std::cout << tree.contains(10) << "\n";
    //std::cout << tree.insert(10) << "\n";
    std::cout << tree.size() << "\n";



    std::cout << tree.insert(4) << "\n";
    //std::cout << tree.contains(4) << "\n";
    //std::cout << tree.insert(4) << "\n";
    std::cout << tree.size() << "\n";



    std::cout << tree.remove(5) << "\n";
    std::cout << tree.remove(5) << "\n";


    std::cout << tree.insert(6) << "\n";
    std::cout << tree.size() << "\n";
    std::cout << tree.insert(8) << "\n";
    std::cout << tree.size() << "\n";
    std::cout << tree.insert(1) << "\n";
    std::cout << tree.size() << "\n";


    std::cout << tree.contains(1) << "\n";
    std::cout << tree.contains(2) << "\n";
    std::cout << tree.contains(3) << "\n";
    std::cout << tree.contains(4) << "\n";
    std::cout << tree.contains(5) << "\n";
    std::cout << tree.contains(6) << "\n";
    std::cout << tree.contains(7) << "\n";
    std::cout << tree.contains(8) << "\n";
    std::cout << tree.contains(9) << "\n";
    std::cout << tree.contains(10) << "\n";

    std::vector<int> v = tree.values();
    std::cout << v.size() << "\n"; 

    std::cout << tree.size() << "\n";
    /*std::cout << tree.remove(1) << "\n";
    std::cout << tree.contains(1) << "\n";
    std::cout << tree.remove(1) << "\n";

     std::cout << tree.remove(2) << "\n";
    std::cout << tree.contains(2) << "\n";
    std::cout << tree.remove(2) << "\n";

     std::cout << tree.remove(3) << "\n";
    std::cout << tree.contains(3) << "\n";
    std::cout << tree.remove(3) << "\n";*/
}
