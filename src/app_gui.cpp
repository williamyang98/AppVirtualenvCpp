#include <optional>
#include <string>
#include <array>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "app.h"
#include "font_awesome_definitions.h"
#include "utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <processenv.h>
#include <ShObjIdl.h>
#include <shtypes.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace app::gui {

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
static std::optional<config_list_t::iterator> RenderManagedConfig(App &main_app, ManagedConfig &managed_cfg, config_list_t::iterator &it);
static void RenderAppConfigCreatorPopup(App &main_app, const char *label);
static void RenderManagedConfigPopup(App &main_app, ManagedConfig &managed_cfg);
static void RenderAppConfigEditForm(ManagedConfig &managed_cfg);
static void RenderWarnings(App &main_app);
static void RenderCriticalErrors(App &main_app);

void RenderApp(App &main_app, const char *label) {
    ImGui::PushID(label);
    ImGuiWindowFlags win_flags = ImGuiWindowFlags_MenuBar;
    ImGui::Begin("Applications", NULL, win_flags);

    bool is_any_dirty = false;
    for (auto &cfg: main_app.GetConfigs()) {
        is_any_dirty = is_any_dirty || cfg->IsDirty();
    }
    is_any_dirty = is_any_dirty || main_app.GetIsConfligListDirtied();

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
                        main_app.GetConfigs().clear();
                    }
                }
            }

            if (is_any_dirty) {
                ImGui::Separator();
                if (ImGui::MenuItem("Save all changes")) {
                    for (auto &cfg: main_app.GetConfigs()) {
                        cfg->ApplyChanges();
                    }
                    main_app.ApplyConfigListChanges();
                    main_app.save_configs();
                }
                if (ImGui::MenuItem("Revert all changes")) {
                    main_app.RevertConfigListChanges();
                    for (auto &cfg: main_app.GetConfigs()) {
                        cfg->RevertChanges();
                    }
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
        const char *apps_tab_suffix = is_any_dirty ? " *" : "";
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

            if (proc->GetIsRunning()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0,255,0).Value);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255,0,0).Value);
            }
            ImGui::Text(ICON_FA_CIRCLE);
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (ImGui::Selectable(proc->GetName().c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_pid = pid;
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
        char *buffer_end = buffer_begin + scroll_buffer.GetReadSize();
        ImGui::TextUnformatted(buffer_begin, buffer_end);

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
        auto &cfgs = main_app.GetConfigs();
        auto it = cfgs.begin();
        auto end = cfgs.end();

        while (it != end) {
            auto &managed_cfg = *it;
            auto &cfg = managed_cfg->GetConfig();
            auto name = cfg.name.c_str();
            if (!filter.PassFilter(name)) {
                continue;
            }

            ImGui::PushID(row_id++);
            auto opt = RenderManagedConfig(main_app, *managed_cfg, it);
            if (!opt) {
                ++it;
            } else {
                it = opt.value();
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

std::optional<config_list_t::iterator> RenderManagedConfig(App &main_app, ManagedConfig &managed_cfg, config_list_t::iterator &it) {
    config_list_t::iterator new_it;
    bool is_deleted = false;

    auto &cfg = managed_cfg.GetConfig();

    ImGui::TableNextRow();

    // gold colour for dirtied configs
    static const ImU32 cell_bg_color = ImGui::GetColorU32(ImVec4(1.0f, 0.84f, 0.0f, 0.4f));
    if (managed_cfg.IsDirty()) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, cell_bg_color);
    }

    ImGui::TableSetColumnIndex(0);
    ImGui::TextWrapped(cfg.name.c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped(cfg.username.c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped(cfg.env_name.c_str());
    ImGui::TableSetColumnIndex(3);
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
        new_it = main_app.RemoveConfigFromList(it);
        is_deleted = true;
    }

    bool is_open = true;
    if (ImGui::BeginPopupModal(popup_name, &is_open)) {
        RenderManagedConfigPopup(main_app, managed_cfg);
        ImGui::EndPopup();
    }

    if (!is_deleted) {
        return {};
    } else {
        return new_it;
    }
}

void RenderAppConfigEditForm(ManagedConfig &managed_cfg) {
    auto &cfg = managed_cfg.GetConfig();

    auto render_path_edit = [&managed_cfg](std::string &s_in, const char *id, const bool expand_flag=false) {
        ImGui::PushID(id);
        ImGui::BeginGroup(); 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));

        if (expand_flag) {
            float button_width = ImGui::CalcTextSize(" .. ").x;
            ImGui::PushItemWidth(-button_width);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_text", &s_in)) {
            managed_cfg.SetDirtyFlag();
        }
        ImGui::PopStyleVar();
        if (expand_flag) {
            ImGui::PopItemWidth();
        }

        ImGui::SameLine();
        bool button_state = ImGui::Button("..");

        ImGui::PopStyleVar();
        ImGui::EndGroup();
        ImGui::PopID();

        return button_state;
    };

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
            managed_cfg.SetDirtyFlag();
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
            managed_cfg.SetDirtyFlag();
        }
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // environment directory
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Environment");
        ImGui::TableSetColumnIndex(1);
        if (render_path_edit(cfg.env_parent_dir, "##edit_env_parent_dir")) {
            auto dialog = CoFileDialog();
            dialog->SetOptions(FOS_PICKFOLDERS);
            auto opt = dialog.open();
            if (opt) {
                cfg.env_parent_dir = std::move(opt.value());
                managed_cfg.SetDirtyFlag();
            }
        }
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_env", &cfg.env_name)) {
            managed_cfg.SetDirtyFlag();
        }
        ImGui::PopStyleVar();

        // executable path
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Executable");
        ImGui::TableSetColumnIndex(1);
        if (render_path_edit(cfg.exec_path, "##edit_exec_path", true)) {
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
                managed_cfg.SetDirtyFlag();
            }
        }

        // args
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Args");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        if (ImGui::InputText("##edit_args", &cfg.args)) {
            managed_cfg.SetDirtyFlag();
        }
        ImGui::PopStyleVar();
        ImGui::PopItemWidth();

        // configuration file
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Config file");
        ImGui::TableSetColumnIndex(1);
        if (render_path_edit(cfg.env_config_path, "##edit_env_config_path", true)) {
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
                managed_cfg.SetDirtyFlag();
            }
        }

        ImGui::EndTable();
    }
}

void RenderManagedConfigPopup(App &main_app, ManagedConfig &managed_cfg) {
    auto &cfg = managed_cfg.GetConfig();

    RenderAppConfigEditForm(managed_cfg);

    ImGui::Separator();

    if (managed_cfg.IsDirty()) {
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
    auto &managed_cfg = main_app.GetCreatorConfig();

    bool is_open = true;
    if (ImGui::BeginPopupModal(label, &is_open)) {
        RenderAppConfigEditForm(managed_cfg);
        ImGui::Separator();
        if (ImGui::Button("Create and add")) {
            main_app.AddConfigToList(managed_cfg.GetConfig());
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