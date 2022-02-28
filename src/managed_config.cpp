#include "managed_config.h"

namespace app {

// managed config
ManagedConfig::ManagedConfig() {
    m_status = Status::UNTRACKED;
    m_is_pending_delete = false;
    m_parent = nullptr;
}

ManagedConfig::ManagedConfig(AppConfig &cfg) {
    m_cfg = cfg;
    m_unchanged_cfg = cfg;
    m_status = Status::UNTRACKED;
    m_is_pending_delete = false;
    m_parent = nullptr;
}

ManagedConfig::ManagedConfig(AppConfig &cfg, ManagedConfigList *parent) {
    m_cfg = cfg;
    m_unchanged_cfg = cfg;
    m_status = Status::UNTRACKED;
    m_is_pending_delete = false;
    m_parent = parent;
    assert(m_parent != nullptr);
}

void ManagedConfig::SetStatus(Status status) {
    // if we edited an untracked config, we still treat it as untracked
    if ((m_status == Status::UNTRACKED) && (status == Status::CHANGED)) {
        return;
    }

    m_status = status;
}

void ManagedConfig::SetIsPendingDelete(bool is_delete) {
    m_is_pending_delete = is_delete;
}

bool ManagedConfig::RevertChanges() {
    // cannot revert changes on a brand new config    
    if ((m_status == Status::UNTRACKED) || (m_status == Status::NONE)) {
        return false;
    }

    m_cfg = m_unchanged_cfg;
    m_status = Status::NONE;
    m_is_pending_delete = false;
    return true;
}

bool ManagedConfig::ApplyChanges() {
    // we can save changes to those which are untracked or have changes
    if ((m_status != Status::UNTRACKED) && (m_status != Status::CHANGED)) {
        return false;
    }

    m_unchanged_cfg = m_cfg;
    m_status = Status::NONE;
    m_is_pending_delete = false;
    if (m_parent) {
        m_parent->SetPendingSave();
    }
    return true;
}

// managed config list
ManagedConfigList::ManagedConfigList() {
    m_is_pending_save = false;
}

bool ManagedConfigList::IsDirty() const {
    for (auto &cfg: m_configs) {
        auto status = cfg->GetStatus();
        if ((status != ManagedConfig::Status::NONE) || cfg->IsPendingDelete()) {
            return true;
        }
    }
    return false;
}

bool ManagedConfigList::RevertChanges() {
    // a managed config list will revert changes to additions and deletions
    auto it = m_configs.begin();
    auto end = m_configs.end();

    while (it != end) {
        auto &cfg = *it;
        auto status = cfg->GetStatus();
        // revert all deletes
        cfg->SetIsPendingDelete(false);

        switch (status) {
        case ManagedConfig::Status::NONE: 
        case ManagedConfig::Status::CHANGED:
            cfg->RevertChanges();
            ++it;
            break;
        case ManagedConfig::Status::UNTRACKED:
        default:
            it = m_configs.erase(it);
            break;
        }
    }
    return true;
}

bool ManagedConfigList::ApplyChanges() {
    auto it = m_configs.begin();
    auto end = m_configs.end();

    while (it != end) {
        auto &cfg = *it;

        if (cfg->IsPendingDelete()) {
            it = m_configs.erase(it);
            continue;
        }

        cfg->ApplyChanges();
        ++it;
    }
    m_is_pending_save = true;
    return true;
}

void ManagedConfigList::Add(AppConfig &cfg) {
    m_configs.push_back(std::make_shared<ManagedConfig>(cfg, this));
}

void ManagedConfigList::Clear() {
    m_configs.clear();
    m_is_pending_save = false;
}

void ManagedConfigList::CommitSave() {
    m_is_pending_save = false;
}

};
