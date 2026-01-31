#pragma once

#include "Commands.h"
#include "eyelib/world/Transactions/Transaction.h"

namespace eyelib {
namespace api {
namespace world {

class Transaction {
public:
    void Add(CommandPtr command);
    ::eyelib::world::Transaction&& TakeInternal();

private:
    ::eyelib::world::Transaction m_tx;
};

} // namespace world
} // namespace api
} // namespace eyelib
