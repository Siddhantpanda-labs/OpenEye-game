#include "TransactionManager.h"

namespace eyelib {
namespace world {

void TransactionManager::Begin() {
    if (!m_open.has_value()) {
        m_open = Transaction{};
    }
}

std::optional<Transaction> TransactionManager::Commit() {
    if (!m_open.has_value()) {
        return std::nullopt;
    }
    auto out = std::move(m_open);
    m_open.reset();
    return out;
}

void TransactionManager::Rollback() {
    m_open.reset();
}

bool TransactionManager::IsOpen() const {
    return m_open.has_value();
}

void TransactionManager::Add(std::unique_ptr<Command> command) {
    if (!m_open.has_value()) {
        return;
    }
    m_open->Add(std::move(command));
}

void TransactionManager::RecordApplied(Transaction transaction) {
    m_undo.push_back(std::move(transaction));
    m_redo.clear();
}

bool TransactionManager::Undo(WorldState& state) {
    if (m_undo.empty()) {
        return false;
    }
    Transaction tx = std::move(m_undo.back());
    m_undo.pop_back();
    const bool ok = tx.Revert(state);
    m_redo.push_back(std::move(tx));
    return ok;
}

bool TransactionManager::Redo(WorldState& state) {
    if (m_redo.empty()) {
        return false;
    }
    Transaction tx = std::move(m_redo.back());
    m_redo.pop_back();
    const bool ok = tx.Apply(state);
    m_undo.push_back(std::move(tx));
    return ok;
}

} // namespace world
} // namespace eyelib
