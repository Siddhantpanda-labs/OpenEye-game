#include "Transactions.h"

namespace eyelib {
namespace api {
namespace world {

void Transaction::Add(CommandPtr command) {
    if (command) {
        m_tx.Add(std::move(command));
    }
}

::eyelib::world::Transaction&& Transaction::TakeInternal() {
    return std::move(m_tx);
}

} // namespace world
} // namespace api
} // namespace eyelib
