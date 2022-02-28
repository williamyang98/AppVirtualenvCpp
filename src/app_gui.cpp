#include <string>
#include <array>
#include <filesystem>
#include <optional>
#include <functional>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "app.h"
#include "font_awesome_definitions.h"
#include "utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>
#include <shobjidl.h>
#include <shtypes.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace app::gui {

namespace fs = std::filesystem;

// helper object for creating windows file dialogs
class CoFileDialog 
{
public:
    CoFileDialog() {
        HRESULT hr = 
            CoCreateInstance(
                CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                IID_IFileOpenDialog, reinterpret_cast<void**>(&m_dialog));
        if (!SUCCEEDED(hr)) {
            throw std::runtime_error("Failed to create file dialog object");
        }
    }
    std::optional<std::string> open() {
        HRESULT hr;

        // open the dialog
        hr = m_dialog->Show(NULL);
        if (!SUCCEEDED(hr)) return {};

        IShellItem *pItem;
        hr = m_dialog->GetResult(&pItem);
        if (!SUCCEEDED(hr)) return {};

        PWSTR pFilepath;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pFilepath);
        if (!SUCCEEDED(hr)) return {};

        std::wstring ws(pFilepath);
        return utility::wide_string_to_string(ws);
    }   
    inline IFileOpenDialog* operator->() const { return m_dialog; }
private:
    IFileOpenDialog *m_dialog;
};

static void RenderAppsTab(App &main_app);
static void RenderProcessesTab(App &main_app);

static void RenderManagedConfigList(App &main_app);
static void RenderManagedConfig(App &main_app, ManagedConfig &managed_cfg);
static void RenderAppConfigCreatorPopup(App &main_app, const char *label);
static void RenderManagedConfigPopup(App &main_app, ManagedConfig &managed_cfg);
static void RenderAppConfigEditForm(ManagedConfig &managed_cfg);
static void RenderWarnings(App &main_app);
static void RenderCriticalErrors(App &main_app);

void RenderApp(App &main_app, const char *label) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushID(label);
    ImGuiWindowFlags win_flags = 
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("##Applications", NULL, win_flags);

    auto &managed_configs = main_app.m_managed_configs;

    // menu bar
    const char *app_create_cfg_label = "Add app###application editor adder";
    bool app_creator_opened = false;

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open apps")) {
                auto dialog = CoFileDialog();
                auto file_types = std::array<_COMDLG_FILTERSPEC, 2> {{
                    { L"All Files", L"*" },
                    { L"JSON", L"*.json" }
                }};
                dialog->SetFileTypes(file_types.size(), file_types.data());
                dialog->SetFileTypeIndex(2);
                auto opt = dialog.open();
                if (opt) {
                    // if failed to load app config, we reset it all
                    if (!main_app.open_app_config(opt.value())) {
                        managed_configs.Clear();
                    }
                }
            }

            if (managed_configs.IsDirty()) {
                ImGui::Separator();
                if (ImGui::MenuItem("Save all changes")) {
                    managed_configs.ApplyChanges();
                    main_app.save_configs();
                }
                if (ImGui::MenuItem("Revert all changes")) {
                    managed_configs.RevertChanges();
                }
            }


            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Add app")) {
            app_creator_opened = true;
        }
        ImGui::EndMenuBar();
    }

    if (ImGui::BeginTabBar("##main_tab_bar")) {
        const char *apps_tab_suffix = managed_configs.IsDirty() ? " *" : "";
        auto apps_label = fmt::format("Applications{}###apps_tab", apps_tab_suffix); 
        if (ImGui::BeginTabItem(apps_label.c_str())) {
            RenderAppsTab(main_app);
            ImGui::EndTabItem();
        }

        int total_processes = main_app.m_processes.size();
        auto tab_processes_label = fmt::format("Processes ({:d})###processes_tab", total_processes); 
        if (ImGui::BeginTabItem(tab_processes_label.c_str())) {
            RenderProcessesTab(main_app);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();

    if (app_creator_opened) {
        ImGui::OpenPopup(app_create_cfg_label);
    }

    RenderCriticalErrors(main_app);
    RenderAppConfigCreatorPopup(main_app, app_create_cfg_label);

    ImGui::PopID();
}

void RenderAppsTab(App &main_app) {
    // configs list 
    float alpha = 0.7f;
    auto left_panel_size = ImVec2(ImGui::GetContentRegionAvail().x*alpha, 0);

    ImGuiWindowFlags flags = 0;
    ImGui::BeginChild("##configs list", left_panel_size, true, flags);
    RenderManagedConfigList(main_app);
    ImGui::EndChild();

    ImGui::SameLine();

    // errors list
    ImGui::BeginChild("##warnings list", ImVec2(0,0), true, flags);
    RenderWarnings(main_app);
    ImGui::EndChild();
}

void RenderProcessesTab(App &main_app) {
    // configs list 
    float alpha = 0.3f;
    auto left_panel_size = ImVec2(ImGui::GetContentRegionAvail().x*alpha, 0);

    auto &processes = main_app.m_processes;

    ImGuiWindowFlags flags = 0;
    ImGui::BeginChild("##process_list_panel", left_panel_size, true, flags);

    static size_t selected_pid = 0;
    
    if (ImGui::BeginListBox("##process_list", ImVec2(-1, -1))) {
        size_t pid = 0;
        for (auto &proc: processes) {
            bool is_selected = (pid == selected_pid);
            ImGui::PushID(pid);
            ImGui::PushItemWidth(-1.0f);

            const auto proc_state = proc->GetState();

            switch (proc_state) {
                case AppProcess::State::RUNNING:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0,255,0).Value);
                    break;
                case AppProcess::State::TERMINATING:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255,215,0).Value);
                    break;
                case AppProcess::State::TERMINATED:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255,0,0).Value);
                    break;
                default:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(60,60,255).Value);
                    break;
            }

            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (ImGui::Selectable(proc->GetName().c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_pid = pid;
            }

            // options while process is running
            if (proc_state == AppProcess::State::RUNNING) {
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Terminate")) {
                        proc->Terminate();
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::PopItemWidth();
            ImGui::PopID();

            pid++;
        }
        ImGui::EndListBox();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // errors list
    flags = ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    ImGui::BeginChild("##process_buffer_panel", ImVec2(0,0), true, flags);
    bool pid_outside_range = (selected_pid < 0) || (selected_pid >= processes.size());
    if (pid_outside_range) {
        ImGui::Text("Select a process to view buffer");
    } else {
        auto &proc = processes[selected_pid];
        auto &scroll_buffer = proc->GetBuffer();
        char *buffer_begin = scroll_buffer.GetReadBuffer();
        size_t buffer_length = scroll_buffer.GetReadSize();
        char *buffer_end = buffer_begin + buffer_length;
        ImGui::TextUnformatted(buffer_begin, buffer_end);
        // copy process text to clipboard
        if (ImGui::BeginPopupContextItem("##buffer_text_context_menu")) {
            if (ImGui::MenuItem("Copy")) {
                utility::CopyToClipboard(buffer_begin, buffer_length);
            }
            ImGui::EndPopup();
        }

    }
    ImGui::EndChild();
}

void RenderManagedConfigList(App &main_app) {
    // filtered table
    static ImGuiTextFilter filter;
    filter.Draw();

    ImGui::Separator();

    ImGuiTableFlags table_flags = 
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##app config table", 4, table_flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Username", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Env", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int row_id = 0;
        auto &managed_configs = main_app.m_managed_configs;
        auto &cfgs = managed_configs.GetConfigs();

        for (auto managed_cfg: managed_configs.GetConfigs()) {
            auto &cfg = managed_cfg->GetConfig();
            auto name = cfg.name.c_str();
            if (!filter.PassFilter(name)) {
                continue;
            }

            // hide these zombie untracked configs which were deleted
            auto status = managed_cfg->GetStatus();
            if ((status == ManagedConfig::Status::UNTRACKED) &&
                (managed_cfg->IsPendingDelete())) 
            {
                continue;
            }

            ImGui::PushID(row_id++);
            RenderManagedConfig(main_app, *managed_cfg);
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void RenderManagedConfig(App &main_app, ManagedConfig &managed_cfg) {
    auto &cfg = managed_cfg.GetConfig();

    ImGui::TableNextRow();

    if (managed_cfg.IsPendingDelete()) {
        ImGui::TableSetBgColor(
            ImGuiTableBgTarget_RowBg0, 
            ImGui::GetColorU32(ImVec4(1.0f, 0.0f, 0.0f, 0.4f)));
    } else {
        auto status = managed_cfg.GetStatus();
        switch (status) {
        case ManagedConfig::Status::CHANGED:
            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0, 
                ImGui::GetColorU32(ImVec4(1.0f, 0.84f, 0.0f, 0.4f)));
            break;
        case ManagedConfig::Status::UNTRACKED:
            ImGui::TableSetBgColor(
                ImGuiTableBgTarget_RowBg0, 
                ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 0.0f, 0.4f)));
            break;
        default:
            break;
        }
    }

    ImGui::TableSetColumnIndex(0);
    ImGui::TextWrapped(cfg.name.c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped(cfg.username.c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped(cfg.env_name.c_str());
    ImGui::TableSetColumnIndex(3);

    // if the entry has been deleted, we render a restore button instead
    if (managed_cfg.IsPendingDelete()) {
        if (ImGui::Button("Restore")) {
            managed_cfg.SetIsPendingDelete(false);
        }
        return;
    }

    if (ImGui::Button("Launch")) {
        main_app.launch_app(cfg);
    }

    const char *popup_name = "Edit Config###edit config popup";
    ImGui::SameLine();
    if (ImGui::Button("Edit")) {
        ImGui::OpenPopup(popup_name);
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        managed_cfg.SetIsPendingDelete(true);
    }

    bool is_open = true;
    if (ImGui::BeginPopupModal(popup_name, &is_open)) {
        RenderManagedConfigPopup(main_app, managed_cfg);
        ImGui::EndPopup();
    }
}

// helpers for rendering an editable path
struct PathEditCallbacks {
    std::function<void (void)> on_edit_callback;
    std::function<void (void)> open_dialog;
    std::function<void (void)> open_get_relative_path;
    std::function<void (void)> open_get_absolute_path;
};

void RenderPathEdit(std::string &s_in, const char *id, PathEditCallbacks &&cbs, const bool expand_flag=true) {
    ImGui::PushID(id);
    ImGui::BeginGroup(); 
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

    if (expand_flag) {
        float button_width = ImGui::CalcTextSize(" .. ").x;
        ImGui::PushItemWidth(-button_width);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (ImGui::InputText("##edit_text", &s_in)) {
        cbs.on_edit_callback();
    }
    ImGui::PopStyleVar();
    if (expand_flag) {
        ImGui::PopItemWidth();
    }

    ImGui::SameLine();
    if (ImGui::Button("..")) {
        cbs.open_dialog();
    }
    if (ImGui::BeginPopupContextItem("##path_context_menu")) {
        if (ImGui::MenuItem("Get relative path")) {
            cbs.open_get_relative_path();
        }
        if (ImGui::MenuItem("Get absolute path")) {
            cbs.open_get_absolute_path();
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
    ImGui::EndGroup();
    ImGui::PopID();
}


// find path relative to execution path
std::optional<std::string> FindRelativePath(const std::string &target_filepath) {
    static const auto cwd = fs::current_path();
    fs::path relative_path = fs::path(target_filepath).lexically_proximate(cwd);
    std::string relative_path_str = relative_path.string();
    if (relative_path_str.compare(target_filepath) == 0) {
        return {};
    }
    return relative_path_str;
}

// get the absolute path
std::optional<std::string> GetAbsolutePath(const std::string &f_in) {
    std::string absolute_path_str = fs::absolute(fs::path(f_in)).string();
    if (absolute_path_str.compare(f_in) == 0) {
        return {};
    }
    return absolute_path_str;
};

void RenderAppConfigEditForm(ManagedConfig &managed_cfg) {
    auto &cfg = managed_cfg.GetConfig();

    ImGuiTableFlags flags = 
        ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("Edit Config Table", 2, flags)) 
    {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // configuration name
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_name", &cfg.name)) {
            managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
        }
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // username
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Username");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_username", &cfg.username)) {
            managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
        }
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // environment directory
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Environment");
        ImGui::TableSetColumnIndex(1);
        RenderPathEdit(cfg.env_parent_dir, "##edit_env_parent_dir", {
            [&managed_cfg]() { managed_cfg.SetStatus(ManagedConfig::Status::CHANGED); },
            [&managed_cfg, &cfg]() {  
                auto dialog = CoFileDialog();
                dialog->SetOptions(FOS_PICKFOLDERS);
                auto opt = dialog.open();
                if (opt) {
                    cfg.env_parent_dir = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {  
                auto opt = FindRelativePath(cfg.env_parent_dir);
                if (opt) {
                    cfg.env_parent_dir = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() { 
                auto opt = GetAbsolutePath(cfg.env_parent_dir);
                if (opt) {
                    cfg.env_parent_dir = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            }
        }, false);
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_env", &cfg.env_name)) {
            managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
        }
        ImGui::PopStyleVar();

        // executable path
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Executable");
        ImGui::TableSetColumnIndex(1);
        RenderPathEdit(cfg.exec_path, "##edit_exec_path", {
            [&managed_cfg]() { managed_cfg.SetStatus(ManagedConfig::Status::CHANGED); },
            [&managed_cfg, &cfg]() {  
                auto dialog = CoFileDialog();
                auto file_types = std::array<_COMDLG_FILTERSPEC, 2> {{
                    { L"All Files", L"*" },
                    { L"Applications", L"*.exe" }
                }};
                dialog->SetFileTypes(file_types.size(), file_types.data());
                dialog->SetFileTypeIndex(2);
                auto opt = dialog.open();
                if (opt) {
                    cfg.exec_path = std::move(opt.value());
                    // when we select a new executable, we automatically set the current working directory
                    cfg.exec_cwd = std::move(fs::path(cfg.exec_path).remove_filename().string());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {  
                auto opt = FindRelativePath(cfg.exec_path);
                if (opt) {
                    cfg.exec_path = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {  
                auto opt = GetAbsolutePath(cfg.exec_path);
                if (opt) {
                    cfg.exec_path = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            }
        });

        // executable cwd
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("CWD");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Current working directory");
            ImGui::EndTooltip();
        }
        ImGui::TableSetColumnIndex(1);
        RenderPathEdit(cfg.exec_cwd, "##edit_exec_cwd", {
            [&managed_cfg]() { managed_cfg.SetStatus(ManagedConfig::Status::CHANGED); },
            [&managed_cfg, &cfg]() {
                auto dialog = CoFileDialog();
                dialog->SetOptions(FOS_PICKFOLDERS);
                auto opt = dialog.open();
                if (opt) {
                    cfg.exec_cwd = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {
                auto opt = FindRelativePath(cfg.exec_cwd);
                if (opt) {
                    cfg.exec_cwd = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {
                auto opt = GetAbsolutePath(cfg.exec_cwd);
                if (opt) {
                    cfg.exec_cwd = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            }
        });

        // args
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Args");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_args", &cfg.args)) {
            managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
        }
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // configuration file
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Config file");
        ImGui::TableSetColumnIndex(1);
        RenderPathEdit(cfg.env_config_path, "##edit_env_config_path", {
            [&managed_cfg]() { managed_cfg.SetStatus(ManagedConfig::Status::CHANGED); },
            [&managed_cfg, &cfg]() {
                auto dialog = CoFileDialog();
                auto file_types = std::array<_COMDLG_FILTERSPEC, 2> {{
                    { L"All Files", L"*" },
                    { L"JSON", L"*.json" }
                }};
                dialog->SetFileTypes(file_types.size(), file_types.data());
                dialog->SetFileTypeIndex(2);
                auto opt = dialog.open();
                if (opt) {
                    cfg.env_config_path = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {
                auto opt = FindRelativePath(cfg.env_config_path);
                if (opt) {
                    cfg.env_config_path = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            },
            [&managed_cfg, &cfg]() {
                auto opt = GetAbsolutePath(cfg.env_config_path);
                if (opt) {
                    cfg.env_config_path = std::move(opt.value());
                    managed_cfg.SetStatus(ManagedConfig::Status::CHANGED);
                }
            }
        });

        ImGui::EndTable();
    }
}

void RenderManagedConfigPopup(App &main_app, ManagedConfig &managed_cfg) {
    RenderAppConfigEditForm(managed_cfg);

    auto status = managed_cfg.GetStatus();

    if ((status & ManagedConfig::Status::CHANGED)) {
        ImGui::Separator();
        if (ImGui::Button("Save Changes##save_change_button")) {
            managed_cfg.ApplyChanges();
            main_app.save_configs();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert Changes##revert_changes_button")) {
            managed_cfg.RevertChanges(); 
        }
    }
}

void RenderAppConfigCreatorPopup(App &main_app, const char *label) {
    auto &creator_cfg = main_app.GetCreatorConfig();

    bool is_open = true;
    if (ImGui::BeginPopupModal(label, &is_open)) {
        RenderAppConfigEditForm(creator_cfg);
        ImGui::Separator();
        if (ImGui::Button("Create and add")) {
            main_app.m_managed_configs.Add(creator_cfg.GetConfig());
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void RenderWarnings(App &main_app) {
    auto &errors = main_app.m_runtime_warnings;

    ImGui::Text("Error List");
    if (ImGui::BeginListBox("##Error List", ImVec2(-1,-1))) {
        
        auto it = errors.begin();
        auto end = errors.end();

        int gid = 0;
        while (it != end) {
            auto &error = *it;

            ImGui::PushID(gid++);

            bool is_pressed = ImGui::Button("X");
            ImGui::SameLine();
            ImGui::TextWrapped(error.c_str());

            if (is_pressed) {
                it = errors.erase(it);
            } else {
                ++it;
            }

            ImGui::PopID();
        }

        ImGui::EndListBox();
    }
}

void RenderCriticalErrors(App &main_app) {
    static const char *modal_title = "Application error###app error modal";

    auto &errors = main_app.m_runtime_errors;
    if (errors.size() == 0) {
        return;
    }

    ImGui::OpenPopup(modal_title);
    
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(modal_title, NULL, 0)) {
        ImGui::Text("The application has encounted an error");
        ImGui::Text("Please restart the application");
        ImGui::Separator();
        if (ImGui::BeginListBox("##app error list", ImVec2(-1,-1))) {
            for (const auto &e: errors) {
                ImGui::TextWrapped(e.c_str());
            }

            ImGui::EndListBox();
        }
        ImGui::EndPopup();
    }
}

}
