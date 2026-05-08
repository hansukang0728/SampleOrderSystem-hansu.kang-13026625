#pragma once
#include <string>

enum class OrderStatus {
    RESERVED,
    REJECTED,
    PRODUCING,
    CONFIRMED,
    RELEASE
};

inline std::string orderStatusToString(OrderStatus s) {
    switch (s) {
        case OrderStatus::RESERVED:  return "RESERVED";
        case OrderStatus::REJECTED:  return "REJECTED";
        case OrderStatus::PRODUCING: return "PRODUCING";
        case OrderStatus::CONFIRMED: return "CONFIRMED";
        case OrderStatus::RELEASE:   return "RELEASE";
        default:                     return "UNKNOWN";
    }
}

struct Order {
    int         id;
    int         sampleId;
    int         quantity;
    std::string customerName;
    OrderStatus status;
};
