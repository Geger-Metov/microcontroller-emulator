#pragma once

#include <vector>

enum class PinDriveMode { HiZ, DriveLow, DriveHigh, PullUp };

enum class WireLevel { Floating, Low, High, Conflict };

class Wire {
public:
  class Endpoint {
  public:
    explicit Endpoint(Wire &wire) : wire_(wire) {
      id_ = wire_.register_endpoint();
    }

    void drive(PinDriveMode mode) { wire_.set_drive(id_, mode); }

    WireLevel read() const { return wire_.resolve(); }

  private:
    Wire &wire_;
    size_t id_;
  };

  size_t register_endpoint() {
    drivers_.push_back(PinDriveMode::HiZ);
    return drivers_.size() - 1;
  }

  void set_drive(size_t id, PinDriveMode mode) { drivers_[id] = mode; }

  WireLevel resolve() const {
    bool low = false;
    bool high = false;
    bool pullup = false;

    for (auto d : drivers_) {
      switch (d) {
      case PinDriveMode::DriveLow:
        low = true;
        break;
      case PinDriveMode::DriveHigh:
        high = true;
        break;
      case PinDriveMode::PullUp:
        pullup = true;
        break;
      case PinDriveMode::HiZ:
        break;
      }
    }
    if (low && high)
      return WireLevel::Conflict;
    if (low)
      return WireLevel::Low;
    if (high)
      return WireLevel::High;
    if (pullup)
      return WireLevel::High;

    return WireLevel::Floating; // все HiZ = плавающий уровень
  }

private:
  std::vector<PinDriveMode> drivers_;
};
