#include "frontend/wizard.h"

#include <float.h>

#include "imgui.h"

#include "app.h"
#include "frontend/ui.h"

extern "C" {
#include "frontend/import.h"
#include "gfx/gl.h"
#include "platform/log.h"
#include "platform/paths.h"
#include "platform/language.h"
#include "frontend/settings.h"
#include "vfs/extract.h"

extern const unsigned char nw_beta_menu_png[];
extern const size_t nw_beta_menu_png_size;
}

#define NW_WIZARD_RESCAN_MS 1000
#define NW_WIZARD_MARGIN 16.0f /* around the window, unscaled */
#define NW_WIZARD_FEEDBACK_URL "https://github.com/denzi-gh/crashwoc-nuworld/issues/new?template=0-beta_feedback.yml"
#define NW_WIZARD_DISCORD_URL "https://discord.gg/kmCPpW4KvJ"

typedef enum NwWizardPage {
    NW_PAGE_PICK,
    NW_PAGE_RUN,
    NW_PAGE_DONE,
    NW_PAGE_FAILED,
    NW_PAGE_WELCOME,
} NwWizardPage;

struct NwWizard {
    SDL_Window *window;
    char *root;
    char *import_dir;
    Uint32 dialog_event; /* file dialog results, pushed from the dialog's thread */
    NwWizardPage page;
    NwImport *job;
    const NwManifest *imported;
    char *image;
    bool had_disc;
    NwImageList list;
    Uint64 scanned;  /* SDL_GetTicks of the last scan, 0 = never */
    char notice[512];
    char error[512];
    GLuint shot; /* 0 = not loaded */
    int shot_w, shot_h;
    bool zoom;
};

static const SDL_DialogFileFilter nw_wizard_filters[] = {
    { "Disc images (.iso, .cue, .bin)", "iso;cue;bin" },
    { "All files", "*" },
};

static const ImVec4 nw_wizard_ok(0.55f, 0.85f, 0.45f, 1.0f);
static const ImVec4 nw_wizard_warn(1.0f, 0.82f, 0.25f, 1.0f);
static const ImVec4 nw_wizard_bad(1.0f, 0.45f, 0.40f, 1.0f);

static const char *nw_wizard_basename(const char *path)
{
    const char *name = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }
    return name;
}

static void nw_wizard_start(NwWizard *w, const char *image)
{
    char *copy = SDL_strdup(image);
    SDL_free(w->image);
    w->image = copy;
    w->notice[0] = '\0';
    w->job = copy ? nw_import_start(copy, w->root, nw_disc_manifests) : NULL;
    if (w->job) {
        w->page = NW_PAGE_RUN;
    } else {
        SDL_strlcpy(w->error, SDL_GetError(), sizeof(w->error));
        w->page = NW_PAGE_FAILED;
    }
}

/* Runs on the dialog's thread. */
static void SDLCALL nw_wizard_picked(void *user, const char *const *files, int filter)
{
    SDL_Event e;
    (void)filter;
    if (files && !files[0]) {
        return; /* cancelled */
    }
    SDL_zero(e);
    e.type = (Uint32)(uintptr_t)user;
    e.user.data1 = files ? SDL_strdup(files[0]) : NULL;
    e.user.data2 = files ? NULL : SDL_strdup(SDL_GetError());
    if (!SDL_PushEvent(&e)) {
        SDL_free(e.user.data1);
        SDL_free(e.user.data2);
    }
}

static void nw_wizard_open_folder(const char *dir)
{
    size_t n = SDL_strlen(dir);
    char *url = (char *)SDL_malloc(n * 3 + 9), *out = url;
    if (!url) {
        return;
    }
    out += SDL_snprintf(out, 9, "file://%s", dir[0] == '/' ? "" : "/");
    for (const unsigned char *p = (const unsigned char *)dir; *p; p++) {
        if (*p == '\\') {
            *out++ = '/';
        } else if (SDL_isalnum(*p) || SDL_strchr("/:-_.~", *p)) {
            *out++ = (char)*p;
        } else {
            out += SDL_snprintf(out, 4, "%%%02X", *p);
        }
    }
    *out = '\0';
    if (!SDL_OpenURL(url)) {
        nw_log_warn(NW_LOG_APP, "cannot open %s: %s", url, SDL_GetError());
    }
    SDL_free(url);
}

NwWizard *nw_wizard_create(SDL_Window *window, const char *root)
{
    NwWizard *w = (NwWizard *)SDL_calloc(1, sizeof(*w));
    if (!w) {
        return NULL;
    }
    w->window = window;
    NwSettings settings;
    nw_settings_defaults(&settings);
    nw_settings_load(&settings, root);
    nw_language_request(settings.language);
    w->root = SDL_strdup(root);
    w->import_dir = nw_paths_dir(root, NW_PATHS_IMPORT, false);
    w->dialog_event = SDL_RegisterEvents(1);
    w->had_disc = nw_extract_has_disc(root);
    if (!w->root || !w->import_dir) {
        nw_wizard_destroy(w);
        return NULL;
    }
    return w;
}

void nw_wizard_destroy(NwWizard *w)
{
    if (!w) {
        return;
    }
    nw_import_free(w->job);
    nw_import_list_free(&w->list);
    if (w->shot) {
        glDeleteTextures(1, &w->shot);
    }
    SDL_free(w->image);
    SDL_free(w->import_dir);
    SDL_free(w->root);
    SDL_free(w);
}

void nw_wizard_event(NwWizard *w, const SDL_Event *e)
{
    if (e->type == SDL_EVENT_DROP_FILE && e->drop.data) {
        if (w->page != NW_PAGE_RUN) {
            nw_wizard_start(w, e->drop.data);
        }
    } else if (w->dialog_event && e->type == w->dialog_event) {
        char *path = (char *)e->user.data1, *why = (char *)e->user.data2;
        if (path && w->page == NW_PAGE_PICK) {
            nw_wizard_start(w, path);
        } else if (why) {
            SDL_snprintf(w->notice, sizeof(w->notice),
                         nw_language_ui_text("No file dialog available (%s). Drop the image here or use the import folder."),
                         why);
        }
        SDL_free(path);
        SDL_free(why);
    }
}

static void nw_wizard_heading(const char *text)
{
    nw_ui_push_font(NW_UI_BOLD, 1.1f);
    ImGui::SeparatorText(nw_language_ui_text(text));
    nw_ui_pop_font();
}

static void nw_wizard_title(const char *text)
{
    nw_ui_push_font(NW_UI_BOLD, 1.35f);
    ImGui::TextUnformatted(nw_language_ui_text(text));
    nw_ui_pop_font();
}

static void nw_wizard_wrapped(const ImVec4 &color, const char *text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped(nw_language_ui_text("%s"), nw_language_ui_text(text));
    ImGui::PopStyleColor();
}

static void nw_wizard_align_right(const char *const *labels, int count)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    float width = 0.0f;
    for (int i = 0; i < count; i++) {
        width += ImGui::CalcTextSize(nw_language_ui_text(labels[i])).x + style.FramePadding.x * 2.0f + (i ? style.ItemSpacing.x : 0.0f);
    }
    float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
    }
}

static NwWizardResult nw_wizard_pick(NwWizard *w)
{
    NwWizardResult result = NW_WIZARD_OPEN;
    Uint64 now = SDL_GetTicks();
    if (!w->scanned || now - w->scanned >= NW_WIZARD_RESCAN_MS) {
        nw_import_list_free(&w->list);
        if (!nw_import_scan(w->import_dir, &w->list)) {
            nw_log_warn(NW_LOG_APP, "cannot list %s: %s", w->import_dir, SDL_GetError());
        }
        w->scanned = now ? now : 1;
    }

    nw_wizard_title("Welcome");
    ImGui::TextWrapped("%s", nw_language_ui_text("NuWorld needs a disc image of your own copy of the game:"));
    ImGui::BulletText("%s", nw_language_ui_text("Crash Bandicoot: The Wrath of Cortex for PS2: PAL v1.03 or v2.01 Platinum (SLES-50386), USA "
                      "v1.00 or v1.01 Greatest Hits (SLUS-20238)"));
    ImGui::BulletText("%s", nw_language_ui_text(".iso, or .bin with its .cue"));

    if (w->had_disc) {
        nw_wizard_wrapped(nw_wizard_ok, "A disc is already imported. It is only replaced if the new import works.");
    }
    if (w->notice[0]) {
        nw_wizard_wrapped(nw_wizard_warn, w->notice);
    }

    nw_wizard_heading("Disc image");
    ImGui::BeginDisabled(!w->dialog_event);
    if (ImGui::Button(nw_ui_label("Choose file..."))) {
        SDL_ShowOpenFileDialog(nw_wizard_picked, (void *)(uintptr_t)w->dialog_event, w->window, nw_wizard_filters,
                               (int)SDL_arraysize(nw_wizard_filters), NULL, false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", nw_language_ui_text("or drop it here"));

    nw_wizard_heading("Import folder");
    ImGui::TextWrapped("%s", nw_language_ui_text("Or copy it into this folder and pick it below (Steam Deck: use Desktop Mode)."));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(nw_language_ui_text("%s"), w->import_dir);
    ImGui::PopStyleColor();
    if (ImGui::Button(nw_ui_label("Open folder"))) {
        nw_wizard_open_folder(w->import_dir);
    }

    const ImGuiTableFlags table = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX;
    if (ImGui::BeginTable("images", 2, table)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed);
        for (size_t i = 0; i < w->list.count; i++) {
            ImGui::PushID((int)i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped(nw_language_ui_text("%s"), w->list.names[i]);
            ImGui::TableNextColumn();
            if (ImGui::Button(nw_ui_label("Import"))) {
                char *path = NULL;
                if (SDL_asprintf(&path, "%s%s", w->import_dir, w->list.names[i]) >= 0) {
                    nw_wizard_start(w, path);
                    SDL_free(path);
                }
            }
            ImGui::PopID();
        }
        if (!w->list.count) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", nw_language_ui_text("No disc images found."));
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped(nw_language_ui_text("Data folder: %s"), w->root);
    ImGui::PopStyleColor();
    static const char *const both[] = { "Start game", "Quit" }, *const quit[] = { "Quit" };
    nw_wizard_align_right(w->had_disc ? both : quit, w->had_disc ? 2 : 1);
    if (w->had_disc) {
        if (ImGui::Button(nw_ui_label("Start game"))) {
            result = NW_WIZARD_CLOSED;
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(nw_ui_label("Quit"))) {
        result = NW_WIZARD_QUIT;
    }
    return result;
}

static void nw_wizard_run(NwWizard *w)
{
    uint64_t done, total;
    NwImportState state = nw_import_poll(w->job, &done, &total);
    if (state != NW_IMPORT_RUNNING) {
        if (state == NW_IMPORT_FAILED) {
            SDL_strlcpy(w->error, nw_import_error(w->job), sizeof(w->error));
        }
        w->imported = state == NW_IMPORT_DONE ? nw_import_manifest(w->job) : NULL;
        nw_import_free(w->job);
        w->job = NULL;
        w->page = state == NW_IMPORT_DONE ? NW_PAGE_DONE : state == NW_IMPORT_FAILED ? NW_PAGE_FAILED : NW_PAGE_PICK;
        if (state == NW_IMPORT_CANCELLED) {
            SDL_strlcpy(w->notice, "Import cancelled. Nothing was changed.", sizeof(w->notice));
        }
        return;
    }

    nw_wizard_title("Importing");
    ImGui::TextWrapped(nw_language_ui_text("%s"), nw_wizard_basename(w->image));
    ImGui::Spacing();
    if (!total) {
        ImGui::TextUnformatted(nw_language_ui_text("Checking the disc..."));
        ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-FLT_MIN, 0.0f), "");
    } else {
        char overlay[32];
        float fraction = (float)((double)done / (double)total);
        SDL_snprintf(overlay, sizeof(overlay), "%d%%", (int)(done * 100 / total));
        ImGui::TextUnformatted(nw_language_ui_text("Copying and checking files..."));
        ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), overlay);
        ImGui::TextDisabled(nw_language_ui_text("%llu of %llu MB"), (unsigned long long)(done >> 20), (unsigned long long)(total >> 20));
    }
    ImGui::Spacing();
    static const char *const cancel[] = { "Cancel" };
    nw_wizard_align_right(cancel, 1);
    if (ImGui::Button(nw_ui_label("Cancel"))) {
        nw_import_cancel(w->job);
    }
}

static void nw_wizard_done(NwWizard *w)
{
    char matches[64];
    SDL_snprintf(matches, sizeof(matches), nw_language_ui_text("Every file matches %s"), w->imported ? w->imported->name : "the disc");
    nw_wizard_title("Disc imported");
    nw_wizard_wrapped(nw_wizard_ok, matches);
    ImGui::TextWrapped(nw_language_ui_text("The game files are in %s" NW_DISC_DIR ". You will not need the disc image again, you can delete it if you want!"),
                       w->root);
    ImGui::Spacing();
    static const char *const next[] = { "Continue" };
    nw_wizard_align_right(next, 1);
    if (ImGui::Button(nw_ui_label("Continue"))) {
        w->page = NW_PAGE_WELCOME;
    }
}

static void nw_wizard_load_shot(NwWizard *w)
{
    SDL_IOStream *io = SDL_IOFromConstMem(nw_beta_menu_png, nw_beta_menu_png_size);
    SDL_Surface *png = io ? SDL_LoadPNG_IO(io, true) : NULL;
    SDL_Surface *rgba = png ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_RGBA32) : NULL;
    SDL_DestroySurface(png);
    if (!rgba || rgba->pitch != rgba->w * 4) {
        nw_log_warn(NW_LOG_APP, "wizard: screenshot: %s", SDL_GetError());
        SDL_DestroySurface(rgba);
        return;
    }
    glGenTextures(1, &w->shot);
    glBindTexture(GL_TEXTURE_2D, w->shot);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    w->shot_w = rgba->w;
    w->shot_h = rgba->h;
    SDL_DestroySurface(rgba);
}

static NwWizardResult nw_wizard_welcome(NwWizard *w)
{
    nw_wizard_title("Welcome to the NuWorld BETA");
    ImGui::TextWrapped("%s", nw_language_ui_text("Thanks for testing! This beta has one goal: fine-tuning N. Hanced Movement, NuWorld's reworked "
                       "movement for Crash, Coco and the vehicles."));

    nw_wizard_heading("Debug menu");
    ImGui::Bullet();
    ImGui::TextWrapped("%s", nw_language_ui_text("Press F1 at any time to open the debug menu. Under Settings and Gameplay you also find video, "
                       "audio, controls and camera, so you can set NuWorld up the way you like."));
    ImGui::Bullet();
    ImGui::TextWrapped("%s", nw_language_ui_text("The debug menu is only for the beta. The first release will have native menus."));
    ImGui::Bullet();
    ImGui::TextWrapped("%s", nw_language_ui_text("Under N. Hanced Movement you can change the parameters of Crash, Coco and every vehicle. Each "
                       "one also has its own key, shown next to it in the menu."));
    ImGui::Bullet();
    ImGui::TextWrapped("%s", nw_language_ui_text("Retail and N. Hanced at the top of a panel switch between the original movement and the "
                       "beta's defaults. Changes are saved automatically."));

    if (!w->shot && w->shot_w >= 0) {
        nw_wizard_load_shot(w);
        if (!w->shot) {
            w->shot_w = -1; /* failed, do not retry every frame */
        }
    }
    ImGui::Spacing();
    if (w->shot) {
        const ImGuiStyle &style = ImGui::GetStyle();
        float limit = ImGui::GetMainViewport()->WorkSize.y - 2.0f * NW_WIZARD_MARGIN * style.FontScaleDpi;
        float footer = ImGui::GetFrameHeight() + style.ItemSpacing.y * 3.0f + style.WindowPadding.y;
        float most = SDL_max(limit - ImGui::GetCursorPosY() - footer, 1.0f);
        float width = ImGui::GetContentRegionAvail().x * 0.6f;
        float height = width * (float)w->shot_h / (float)w->shot_w;
        if (height > most) {
            width = most * (float)w->shot_w / (float)w->shot_h;
            height = most;
        }
        ImGui::Image((ImTextureID)w->shot, ImVec2(width, height));
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("%s", nw_language_ui_text("Click to enlarge"));
        }
        if (ImGui::IsItemClicked()) {
            w->zoom = true;
        }
        ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
    }

    ImGui::BeginGroup();
    nw_wizard_heading("Feedback");
    ImGui::TextWrapped("%s", nw_language_ui_text("Something not working, or a parameter you would like to have? Create an issue on GitHub with "
                       "the Beta Feedback template, or start a thread on the Discord server."));
    if (ImGui::Button(nw_ui_label("Beta Feedback on GitHub"))) {
        SDL_OpenURL(NW_WIZARD_FEEDBACK_URL);
    }
    ImGui::SameLine();
    if (ImGui::Button(nw_ui_label("Discord"))) {
        SDL_OpenURL(NW_WIZARD_DISCORD_URL);
    }
    ImGui::EndGroup();

    ImGui::Spacing();
    static const char *const start[] = { "Start game" };
    nw_wizard_align_right(start, 1);
    return ImGui::Button(nw_ui_label("Start game")) ? NW_WIZARD_CLOSED : NW_WIZARD_OPEN;
}

static void nw_wizard_zoom(NwWizard *w)
{
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    float scale = SDL_min(vp->WorkSize.x / (float)w->shot_w, vp->WorkSize.y / (float)w->shot_h);
    ImVec2 size((float)w->shot_w * scale, (float)w->shot_h * scale);
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.85f));
    ImGui::Begin("##zoom", NULL,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorPos(ImVec2((vp->WorkSize.x - size.x) * 0.5f, (vp->WorkSize.y - size.y) * 0.5f));
    ImGui::Image((ImTextureID)w->shot, size);
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        w->zoom = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        w->zoom = false;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

static void nw_wizard_failed(NwWizard *w)
{
    nw_wizard_title("Import failed");
    ImGui::TextWrapped(nw_language_ui_text("%s"), nw_wizard_basename(w->image ? w->image : ""));
    nw_wizard_wrapped(nw_wizard_bad, w->error);
    ImGui::TextWrapped("%s", nw_language_ui_text("NuWorld runs PAL v1.03 and v2.01 (SLES-50386) and USA v1.00 and v1.01 (SLUS-20238). The data "
                       "folder is unchanged."));
    ImGui::Spacing();
    static const char *const buttons[] = { "Try again", "Back" };
    nw_wizard_align_right(buttons, 2);
    if (ImGui::Button(nw_ui_label("Try again")) && w->image) {
        nw_wizard_start(w, w->image); /* copies the path before freeing the old one */
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button(nw_ui_label("Back"))) {
        w->page = NW_PAGE_PICK;
    }
}

NwWizardResult nw_wizard_draw(NwWizard *w)
{
    NwWizardResult result = NW_WIZARD_OPEN;
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    float scale = ImGui::GetStyle().FontScaleDpi;
    float margin = NW_WIZARD_MARGIN * scale;
    float wide = w->page == NW_PAGE_WELCOME ? 960.0f : 760.0f; /* room for the screenshot */
    float width = SDL_max(SDL_min(vp->WorkSize.x - 2.0f * margin, wide * scale), 240.0f);
    float height = SDL_max(vp->WorkSize.y - 2.0f * margin, 120.0f);

    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, height));
    ImGui::Begin(NW_APP_NAME, NULL,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextDisabled(nw_language_ui_text("%s"), nw_language_ui_text(NW_APP_NAME " - first-time setup"));
    int language = nw_language_preference() == NW_LANGUAGE_PT_BR ? 1 : 0;
    static const char *const languages[] = { "English", "Português (BR)" };
    if (ImGui::Combo("Language / Idioma", &language, languages, 2)) {
        NwSettings settings;
        nw_settings_defaults(&settings);
        nw_settings_load(&settings, w->root);
        settings.language = language ? NW_LANGUAGE_PT_BR : NW_LANGUAGE_DISC;
        nw_language_request(settings.language);
        nw_settings_save(&settings, w->root);
    }
    switch (w->page) {
    case NW_PAGE_PICK:
        result = nw_wizard_pick(w);
        break;
    case NW_PAGE_RUN:
        nw_wizard_run(w);
        break;
    case NW_PAGE_DONE:
        nw_wizard_done(w);
        break;
    case NW_PAGE_FAILED:
        nw_wizard_failed(w);
        break;
    case NW_PAGE_WELCOME:
        result = nw_wizard_welcome(w);
        break;
    }
    ImGui::End();
    if (w->zoom && w->shot) {
        nw_wizard_zoom(w);
    }
    return result;
}
