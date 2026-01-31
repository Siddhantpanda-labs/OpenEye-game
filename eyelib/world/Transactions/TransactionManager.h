#pragma once

#include "Transaction.h"

#include <optional>
#include <vector>

namespace eyelib {
namespace world {

class TransactionManager {
public:
    void Begin();
    std::optional<Transaction> Commit();
    void Rollback();

    bool IsOpen() const;

    void Add(std::unique_ptr<Command> command);

    void RecordApplied(Transaction transaction);
    bool Undo(WorldState& state);
    bool Redo(WorldState& state);

private:
    std::optional<Transaction> m_open;
    std::vector<Transaction> m_undo;
    std::vector<Transaction> m_redo;
};

} // namespace world
} // namespace eyelib
