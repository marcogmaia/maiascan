// Copyright (c) Maia

#pragma once

namespace maia {

class IWidget {
 public:
  virtual ~IWidget() = default;
  virtual void Render() = 0;
};

}  // namespace maia
